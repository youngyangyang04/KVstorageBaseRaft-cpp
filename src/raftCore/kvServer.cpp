#include "kvServer.h"

#include <rpcprovider.h>

#include <algorithm>
#include <memory>
#include <unistd.h>
#include <utility>

#include "mprpcconfig.h"

namespace {
constexpr int kConfigPollIntervalMs = 1000;
constexpr int kMigrationPollIntervalMs = 500;

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

bool KvServer::IsShardReadyLocked(int shardId) {
  auto it = m_shardLifecycle.find(shardId);
  if (it == m_shardLifecycle.end()) {
    return false;
  }
  return it->second == ShardLifecycle::Serving;
}

bool KvServer::IsShardReady(int shardId) {
  std::lock_guard<std::mutex> lg(m_mtx);
  return IsShardReadyLocked(shardId);
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

std::string KvServer::SerializeLastRequestIdMap(const std::unordered_map<std::string, int>& requestMap) const {
  std::stringstream ss;
  boost::archive::text_oarchive oa(ss);
  oa << requestMap;
  return ss.str();
}

std::unordered_map<std::string, int> KvServer::DeserializeLastRequestIdMap(const std::string& serialized) const {
  if (serialized.empty()) {
    return {};
  }
  std::unordered_map<std::string, int> requestMap;
  std::stringstream ss(serialized);
  boost::archive::text_iarchive ia(ss);
  ia >> requestMap;
  return requestMap;
}

void KvServer::ApplyTopologyUpdate(const StaticGroupTopology& topology) {
  std::lock_guard<std::mutex> lg(m_mtx);
  if (topology.configVersion <= m_configVersion) {
    return;
  }

  const std::vector<int> oldOwners = m_shardOwners;
  const int oldConfigVersion = m_configVersion;
  m_configVersion = topology.configVersion;
  m_shardCount = topology.shardCount;
  m_shardOwners = topology.shardOwners;

  for (int shardId = 0; shardId < m_shardCount; ++shardId) {
    if (!IsShardServedByThisGroup(shardId)) {
      m_shardLifecycle.erase(shardId);
      m_shardSourceGroup.erase(shardId);
      continue;
    }

    const bool previouslyOwned =
        shardId < static_cast<int>(oldOwners.size()) && oldOwners[shardId] == m_groupId;
    if (previouslyOwned || oldConfigVersion == 0) {
      EnsureShardState(shardId);
      m_shardLifecycle[shardId] = ShardLifecycle::Serving;
      m_shardSourceGroup.erase(shardId);
      continue;
    }

    if (FindShardState(shardId) != nullptr) {
      m_shardLifecycle[shardId] = ShardLifecycle::Serving;
      m_shardSourceGroup.erase(shardId);
      continue;
    }

    m_shardLifecycle[shardId] = ShardLifecycle::Pulling;
    if (shardId < static_cast<int>(oldOwners.size())) {
      m_shardSourceGroup[shardId] = oldOwners[shardId];
    }
  }
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

void KvServer::ExecuteInstallShardOp(const Op& op) {
  std::lock_guard<std::mutex> lg(m_mtx);
  if (op.ShardId < 0 || !IsShardServedByThisGroup(op.ShardId)) {
    return;
  }

  auto& shardState = EnsureShardState(op.ShardId);
  shardState.skipList = std::make_unique<SkipList<std::string, std::string>>(6);
  shardState.lastRequestId = op.ShardLastRequestId;
  if (!op.ShardData.empty()) {
    shardState.skipList->load_file(op.ShardData);
  }

  m_shardLifecycle[op.ShardId] = ShardLifecycle::Serving;
  m_shardSourceGroup.erase(op.ShardId);
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
  if (!IsShardReady(shardId)) {
    reply->set_err(ErrShardNotReady);
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

  if (op.Operation == "InstallShard") {
    ExecuteInstallShardOp(op);
  } else {
    const int shardId = GetShardIdForKey(op.Key);
    if (IsShardServedByThisGroup(shardId) && !ifRequestDuplicate(shardId, op.ClientId, op.RequestId)) {
      if (op.Operation == "Put") {
        ExecutePutOpOnKVDB(op);
      }
      if (op.Operation == "Append") {
        ExecuteAppendOpOnKVDB(op);
      }
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
  if (!IsShardReady(shardId)) {
    reply->set_err(ErrShardNotReady);
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

void KvServer::FetchShardData(const raftKVRpcProctoc::FetchShardDataArgs* args,
                              raftKVRpcProctoc::FetchShardDataReply* reply) {
  int ignoredTerm = -1;
  bool isLeader = false;
  m_raftNode->GetState(&ignoredTerm, &isLeader);
  if (!isLeader) {
    reply->set_err(ErrWrongLeader);
    return;
  }

  std::lock_guard<std::mutex> lg(m_mtx);
  reply->set_configversion(m_configVersion);
  const auto* shardState = FindShardState(args->shardid());
  if (shardState == nullptr) {
    reply->set_err(OK);
    reply->set_sharddata("");
    reply->set_lastrequestid("");
    return;
  }

  reply->set_err(OK);
  reply->set_sharddata(shardState->skipList->dump_file());
  reply->set_lastrequestid(SerializeLastRequestIdMap(shardState->lastRequestId));
}

bool KvServer::TryFetchShardData(int shardId, int sourceGroupId, int configVersion, std::string* shardData,
                                 std::unordered_map<std::string, int>* lastRequestId) {
  MprpcConfig config;
  config.LoadConfigFile(m_configFileName.c_str());
  const auto topology = LoadStaticGroupTopology(config);
  auto it = topology.groupServers.find(sourceGroupId);
  if (it == topology.groupServers.end()) {
    return false;
  }

  raftKVRpcProctoc::FetchShardDataArgs args;
  args.set_shardid(shardId);
  args.set_configversion(configVersion);

  for (const auto& [ip, port] : it->second) {
    std::unique_ptr<raftKVRpcProctoc::kvServerRpc_Stub> stub(
        new raftKVRpcProctoc::kvServerRpc_Stub(new MprpcChannel(ip, port, false)));
    MprpcController controller;
    raftKVRpcProctoc::FetchShardDataReply reply;
    stub->FetchShardData(&controller, &args, &reply, nullptr);
    if (controller.Failed() || reply.err() == ErrWrongLeader) {
      continue;
    }
    if (reply.err() != OK) {
      return false;
    }

    *shardData = reply.sharddata();
    *lastRequestId = DeserializeLastRequestIdMap(reply.lastrequestid());
    return true;
  }

  return false;
}

void KvServer::ConfigPollLoop() {
  while (true) {
    sleepNMilliseconds(kConfigPollIntervalMs);
    MprpcConfig config;
    config.LoadConfigFile(m_configFileName.c_str());
    ApplyTopologyUpdate(LoadStaticGroupTopology(config));
  }
}

void KvServer::MigrationLoop() {
  while (true) {
    sleepNMilliseconds(kMigrationPollIntervalMs);

    int ignoredTerm = -1;
    bool isLeader = false;
    m_raftNode->GetState(&ignoredTerm, &isLeader);
    if (!isLeader) {
      continue;
    }

    std::vector<std::pair<int, int>> pendingShards;
    int configVersion = 0;
    {
      std::lock_guard<std::mutex> lg(m_mtx);
      configVersion = m_configVersion;
      for (const auto& [shardId, lifecycle] : m_shardLifecycle) {
        if (lifecycle != ShardLifecycle::Pulling) {
          continue;
        }
        auto sourceIt = m_shardSourceGroup.find(shardId);
        if (sourceIt == m_shardSourceGroup.end()) {
          continue;
        }
        pendingShards.emplace_back(shardId, sourceIt->second);
      }
    }

    for (const auto& [shardId, sourceGroupId] : pendingShards) {
      std::string shardData;
      std::unordered_map<std::string, int> lastRequestId;
      if (!TryFetchShardData(shardId, sourceGroupId, configVersion, &shardData, &lastRequestId)) {
        continue;
      }

      Op installOp;
      installOp.Operation = "InstallShard";
      installOp.ClientId = "migration-group-" + std::to_string(m_groupId);
      installOp.RequestId = configVersion * 1000 + shardId;
      installOp.ConfigVersion = configVersion;
      installOp.ShardId = shardId;
      installOp.ShardData = shardData;
      installOp.ShardLastRequestId = lastRequestId;

      int raftIndex = -1;
      int raftTerm = -1;
      bool stillLeader = false;
      m_raftNode->Start(installOp, &raftIndex, &raftTerm, &stillLeader);
      if (!stillLeader) {
        break;
      }
    }
  }
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

void KvServer::FetchShardData(google::protobuf::RpcController* controller,
                              const ::raftKVRpcProctoc::FetchShardDataArgs* request,
                              ::raftKVRpcProctoc::FetchShardDataReply* response,
                              ::google::protobuf::Closure* done) {
  KvServer::FetchShardData(request, response);
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
  m_configVersion = topology.configVersion;
  m_configFileName = std::move(nodeInforFileName);

  for (int shardId = 0; shardId < m_shardCount; ++shardId) {
    if (IsShardServedByThisGroup(shardId)) {
      EnsureShardState(shardId);
      m_shardLifecycle[shardId] = ShardLifecycle::Serving;
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

  std::thread configThread(&KvServer::ConfigPollLoop, this);
  configThread.detach();

  std::thread migrationThread(&KvServer::MigrationLoop, this);
  migrationThread.detach();

  std::thread applyThread(&KvServer::ReadRaftApplyCommandLoop, this);
  applyThread.join();
}
