#include "kvServer.h"

#include <rpcprovider.h>

#include <algorithm>
#include <memory>
#include <unistd.h>
#include <utility>

#include "mprpcconfig.h"

namespace {
std::vector<std::pair<std::string, short>> LoadRaftPeersForGroup(MprpcConfig& config, int groupId) {
  StaticGroupTopology topology = LoadStaticGroupTopology(config);
  auto it = topology.groupServers.find(groupId);
  if (it == topology.groupServers.end()) {
    return {};
  }
  return it->second;
}
}  // namespace

void KvServer::DprintfKVDB() {
  if (!Debug) {
    return;
  }

  std::lock_guard<std::mutex> lg(m_mtx);
  for (const auto& [shardId, shardState] : m_shards) {
    std::cout << "[Shard " << shardId << "]" << std::endl;
    shardState->skipList->display_list();
  }
}

int KvServer::GetShardIdForKey(const std::string& key) const { return KeyToShard(key, m_shardCount); }

bool KvServer::IsShardServedByThisGroup(int shardId) const {
  return shardId >= 0 && shardId < static_cast<int>(m_shardOwners.size()) && m_shardOwners[shardId] == m_groupId;
}

KvServer::ShardState& KvServer::EnsureShardState(int shardId) {
  auto it = m_shards.find(shardId);
  if (it == m_shards.end()) {
    it = m_shards.emplace(shardId, std::make_unique<ShardState>()).first;
  }
  return *it->second;
}

const KvServer::ShardState* KvServer::FindShardState(int shardId) const {
  auto it = m_shards.find(shardId);
  if (it == m_shards.end()) {
    return nullptr;
  }
  return it->second.get();
}

void KvServer::ExecuteAppendOpOnKVDB(const Op& op) {
  const int shardId = GetShardIdForKey(op.Key);
  std::lock_guard<std::mutex> lg(m_mtx);
  auto& shardState = EnsureShardState(shardId);
  std::string mergedValue;
  if (shardState.skipList->search_element(op.Key, mergedValue)) {
    mergedValue += op.Value;
  } else {
    mergedValue = op.Value;
  }
  std::string keyCopy = op.Key;
  shardState.skipList->insert_set_element(keyCopy, mergedValue);
  shardState.lastRequestId[op.ClientId] = op.RequestId;
}

void KvServer::ExecuteGetOpOnKVDB(const Op& op, std::string* value, bool* exist) {
  const int shardId = GetShardIdForKey(op.Key);
  std::lock_guard<std::mutex> lg(m_mtx);
  *value = "";
  *exist = false;

  auto& shardState = EnsureShardState(shardId);
  if (shardState.skipList->search_element(op.Key, *value)) {
    *exist = true;
  }
  shardState.lastRequestId[op.ClientId] = op.RequestId;
}

void KvServer::ExecutePutOpOnKVDB(const Op& op) {
  const int shardId = GetShardIdForKey(op.Key);
  std::lock_guard<std::mutex> lg(m_mtx);
  auto& shardState = EnsureShardState(shardId);
  std::string keyCopy = op.Key;
  std::string valueCopy = op.Value;
  shardState.skipList->insert_set_element(keyCopy, valueCopy);
  shardState.lastRequestId[op.ClientId] = op.RequestId;
}

void KvServer::Get(const raftKVRpcProctoc::GetArgs* args, raftKVRpcProctoc::GetReply* reply) {
  Op op;
  op.Operation = "Get";
  op.Key = args->key();
  op.Value = "";
  op.ClientId = args->clientid();
  op.RequestId = args->requestid();

  const int shardId = GetShardIdForKey(op.Key);
  if (!IsShardServedByThisGroup(shardId)) {
    reply->set_err(ErrWrongGroup);
    return;
  }

  int raftIndex = -1;
  int ignoredTerm = -1;
  bool isLeader = false;
  m_raftNode->Start(op, &raftIndex, &ignoredTerm, &isLeader);

  if (!isLeader) {
    reply->set_err(ErrWrongLeader);
    return;
  }

  m_mtx.lock();
  if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
    waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
  }
  auto chForRaftIndex = waitApplyCh[raftIndex];
  m_mtx.unlock();

  Op raftCommitOp;
  if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
    int ignored = -1;
    bool stillLeader = false;
    m_raftNode->GetState(&ignored, &stillLeader);
    if (ifRequestDuplicate(shardId, op.ClientId, op.RequestId) && stillLeader) {
      std::string value;
      bool exist = false;
      ExecuteGetOpOnKVDB(op, &value, &exist);
      if (exist) {
        reply->set_err(OK);
        reply->set_value(value);
      } else {
        reply->set_err(ErrNoKey);
        reply->set_value("");
      }
    } else {
      reply->set_err(ErrWrongLeader);
    }
  } else {
    if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
      std::string value;
      bool exist = false;
      ExecuteGetOpOnKVDB(op, &value, &exist);
      if (exist) {
        reply->set_err(OK);
        reply->set_value(value);
      } else {
        reply->set_err(ErrNoKey);
        reply->set_value("");
      }
    } else {
      reply->set_err(ErrWrongLeader);
    }
  }

  m_mtx.lock();
  auto tmp = waitApplyCh[raftIndex];
  waitApplyCh.erase(raftIndex);
  delete tmp;
  m_mtx.unlock();
}

void KvServer::GetCommandFromRaft(ApplyMsg message) {
  Op op;
  op.parseFromString(message.Command);

  if (message.CommandIndex <= m_lastSnapShotRaftLogIndex) {
    return;
  }

  const int shardId = GetShardIdForKey(op.Key);
  if (IsShardServedByThisGroup(shardId) && !ifRequestDuplicate(shardId, op.ClientId, op.RequestId)) {
    if (op.Operation == "Put") {
      ExecutePutOpOnKVDB(op);
    }
    if (op.Operation == "Append") {
      ExecuteAppendOpOnKVDB(op);
    }
  }

  if (m_maxRaftState != -1) {
    IfNeedToSendSnapShotCommand(message.CommandIndex, 9);
  }

  SendMessageToWaitChan(op, message.CommandIndex);
}

bool KvServer::ifRequestDuplicate(int shardId, const std::string& clientId, int requestId) {
  std::lock_guard<std::mutex> lg(m_mtx);
  const auto* shardState = FindShardState(shardId);
  if (shardState == nullptr) {
    return false;
  }
  auto it = shardState->lastRequestId.find(clientId);
  if (it == shardState->lastRequestId.end()) {
    return false;
  }
  return requestId <= it->second;
}

void KvServer::PutAppend(const raftKVRpcProctoc::PutAppendArgs* args, raftKVRpcProctoc::PutAppendReply* reply) {
  Op op;
  op.Operation = args->op();
  op.Key = args->key();
  op.Value = args->value();
  op.ClientId = args->clientid();
  op.RequestId = args->requestid();

  const int shardId = GetShardIdForKey(op.Key);
  if (!IsShardServedByThisGroup(shardId)) {
    reply->set_err(ErrWrongGroup);
    return;
  }

  int raftIndex = -1;
  int ignoredTerm = -1;
  bool isLeader = false;
  m_raftNode->Start(op, &raftIndex, &ignoredTerm, &isLeader);

  if (!isLeader) {
    reply->set_err(ErrWrongLeader);
    return;
  }

  m_mtx.lock();
  if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
    waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
  }
  auto chForRaftIndex = waitApplyCh[raftIndex];
  m_mtx.unlock();

  Op raftCommitOp;
  if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
    if (ifRequestDuplicate(shardId, op.ClientId, op.RequestId)) {
      reply->set_err(OK);
    } else {
      reply->set_err(ErrWrongLeader);
    }
  } else {
    if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
      reply->set_err(OK);
    } else {
      reply->set_err(ErrWrongLeader);
    }
  }

  m_mtx.lock();
  auto tmp = waitApplyCh[raftIndex];
  waitApplyCh.erase(raftIndex);
  delete tmp;
  m_mtx.unlock();
}

void KvServer::ReadRaftApplyCommandLoop() {
  while (true) {
    auto message = applyChan->Pop();
    if (message.CommandValid) {
      GetCommandFromRaft(message);
    }
    if (message.SnapshotValid) {
      GetSnapShotFromRaft(message);
    }
  }
}

void KvServer::ReadSnapShotToInstall(std::string snapshot) {
  if (snapshot.empty()) {
    return;
  }
  parseFromString(snapshot);
}

bool KvServer::SendMessageToWaitChan(const Op& op, int raftIndex) {
  std::lock_guard<std::mutex> lg(m_mtx);
  if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
    return false;
  }
  waitApplyCh[raftIndex]->Push(op);
  return true;
}

void KvServer::IfNeedToSendSnapShotCommand(int raftIndex, int proportion) {
  if (m_raftNode->GetRaftStateSize() > m_maxRaftState / static_cast<double>(proportion)) {
    auto snapshot = MakeSnapShot();
    m_raftNode->Snapshot(raftIndex, snapshot);
  }
}

void KvServer::GetSnapShotFromRaft(ApplyMsg message) {
  std::lock_guard<std::mutex> lg(m_mtx);
  if (m_raftNode->CondInstallSnapshot(message.SnapshotTerm, message.SnapshotIndex, message.Snapshot)) {
    ReadSnapShotToInstall(message.Snapshot);
    m_lastSnapShotRaftLogIndex = message.SnapshotIndex;
  }
}

std::string KvServer::MakeSnapShot() {
  std::lock_guard<std::mutex> lg(m_mtx);
  return getSnapshotData();
}

void KvServer::PutAppend(google::protobuf::RpcController* controller, const ::raftKVRpcProctoc::PutAppendArgs* request,
                         ::raftKVRpcProctoc::PutAppendReply* response, ::google::protobuf::Closure* done) {
  KvServer::PutAppend(request, response);
  done->Run();
}

void KvServer::Get(google::protobuf::RpcController* controller, const ::raftKVRpcProctoc::GetArgs* request,
                   ::raftKVRpcProctoc::GetReply* response, ::google::protobuf::Closure* done) {
  KvServer::Get(request, response);
  done->Run();
}

std::string KvServer::getSnapshotData() {
  m_serializedShardData.clear();
  m_lastRequestIdByShard.clear();
  for (const auto& [shardId, shardState] : m_shards) {
    m_serializedShardData[shardId] = shardState->skipList->dump_file();
    m_lastRequestIdByShard[shardId] = shardState->lastRequestId;
  }

  std::stringstream ss;
  boost::archive::text_oarchive oa(ss);
  oa << *this;

  m_serializedShardData.clear();
  m_lastRequestIdByShard.clear();
  return ss.str();
}

void KvServer::parseFromString(const std::string& str) {
  std::stringstream ss(str);
  boost::archive::text_iarchive ia(ss);
  ia >> *this;

  m_shards.clear();
  for (const auto& [shardId, shardData] : m_serializedShardData) {
    auto& shardState = EnsureShardState(shardId);
    shardState.skipList->load_file(shardData);
  }
  for (const auto& [shardId, lastRequestId] : m_lastRequestIdByShard) {
    auto& shardState = EnsureShardState(shardId);
    shardState.lastRequestId = lastRequestId;
  }

  m_serializedShardData.clear();
  m_lastRequestIdByShard.clear();
}

KvServer::KvServer(int me, int groupId, int maxraftstate, std::string nodeInforFileName, short port) {
  MprpcConfig config;
  config.LoadConfigFile(nodeInforFileName.c_str());
  const auto topology = LoadStaticGroupTopology(config);

  m_me = me;
  m_groupId = groupId;
  m_shardCount = topology.shardCount;
  m_shardOwners = topology.shardOwners;
  m_maxRaftState = maxraftstate;
  m_lastSnapShotRaftLogIndex = 0;

  for (int shardId = 0; shardId < m_shardCount; ++shardId) {
    if (IsShardServedByThisGroup(shardId)) {
      EnsureShardState(shardId);
    }
  }

  std::shared_ptr<Persister> persister =
      std::make_shared<Persister>("group" + std::to_string(m_groupId) + "_node" + std::to_string(m_me));
  applyChan = std::make_shared<LockQueue<ApplyMsg>>();
  m_raftNode = std::make_shared<Raft>();

  std::thread t([this, port]() -> void {
    RpcProvider provider;
    provider.NotifyService(this);
    provider.NotifyService(this->m_raftNode.get());
    provider.Run(m_me, port);
  });
  t.detach();

  std::cout << "raftServer group:" << m_groupId << " node:" << m_me
            << " waiting for peer processes before creating raft clients" << std::endl;
  sleep(6);

  auto ipPortVt = LoadRaftPeersForGroup(config, m_groupId);
  std::vector<std::shared_ptr<RaftRpcUtil>> servers;
  for (int i = 0; i < static_cast<int>(ipPortVt.size()); ++i) {
    if (i == m_me) {
      servers.push_back(nullptr);
      continue;
    }
    auto* rpc = new RaftRpcUtil(ipPortVt[i].first, ipPortVt[i].second);
    servers.push_back(std::shared_ptr<RaftRpcUtil>(rpc));
  }

  sleep(std::max(1, static_cast<int>(ipPortVt.size()) - me));
  m_raftNode->init(servers, m_me, persister, applyChan);

  auto snapshot = persister->ReadSnapshot();
  if (!snapshot.empty()) {
    ReadSnapShotToInstall(snapshot);
  }

  std::thread t2(&KvServer::ReadRaftApplyCommandLoop, this);
  t2.join();
}
