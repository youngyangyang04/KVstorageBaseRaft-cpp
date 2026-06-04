#ifndef SKIP_LIST_ON_RAFT_KVSERVER_H
#define SKIP_LIST_ON_RAFT_KVSERVER_H

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "kvServerRPC.pb.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include "raft.h"
#include "shardConfig.h"
#include "skipList.h"

class KvServer : public raftKVRpcProctoc::kvServerRpc {
 private:
  enum class ShardLifecycle {
    Serving = 0,
    Pulling = 1,
  };

  struct ShardState {
    ShardState() : skipList(std::make_unique<SkipList<std::string, std::string>>(6)) {}

    std::unique_ptr<SkipList<std::string, std::string>> skipList;
    std::unordered_map<std::string, int> lastRequestId;
  };

  std::mutex m_mtx;
  int m_me;
  int m_groupId;
  int m_shardCount;
  int m_maxRaftState;
  int m_lastSnapShotRaftLogIndex;
  int m_configVersion;
  std::string m_configFileName;

  std::shared_ptr<Raft> m_raftNode;
  std::shared_ptr<LockQueue<ApplyMsg>> applyChan;

  std::unordered_map<int, std::unique_ptr<ShardState>> m_shards;
  std::unordered_map<int, LockQueue<Op>*> waitApplyCh;
  std::vector<int> m_shardOwners;
  std::unordered_map<int, ShardLifecycle> m_shardLifecycle;
  std::unordered_map<int, int> m_shardSourceGroup;

  std::unordered_map<int, std::string> m_serializedShardData;
  std::unordered_map<int, std::unordered_map<std::string, int>> m_lastRequestIdByShard;

 public:
  KvServer() = delete;

  KvServer(int me, int groupId, int maxraftstate, std::string nodeInforFileName, short port);

  void StartKVServer();
  void DprintfKVDB();

  void ExecuteAppendOpOnKVDB(const Op& op);
  void ExecuteGetOpOnKVDB(const Op& op, std::string* value, bool* exist);
  void ExecutePutOpOnKVDB(const Op& op);
  void ExecuteInstallShardOp(const Op& op);

  void Get(const raftKVRpcProctoc::GetArgs* args, raftKVRpcProctoc::GetReply* reply);
  void GetCommandFromRaft(ApplyMsg message);
  bool ifRequestDuplicate(int shardId, const std::string& clientId, int requestId);
  void PutAppend(const raftKVRpcProctoc::PutAppendArgs* args, raftKVRpcProctoc::PutAppendReply* reply);
  void FetchShardData(const raftKVRpcProctoc::FetchShardDataArgs* args,
                      raftKVRpcProctoc::FetchShardDataReply* reply);
  void ReadRaftApplyCommandLoop();
  void ReadSnapShotToInstall(std::string snapshot);
  bool SendMessageToWaitChan(const Op& op, int raftIndex);
  void IfNeedToSendSnapShotCommand(int raftIndex, int proportion);
  void GetSnapShotFromRaft(ApplyMsg message);
  std::string MakeSnapShot();
  void ConfigPollLoop();
  void MigrationLoop();

  void PutAppend(google::protobuf::RpcController* controller, const ::raftKVRpcProctoc::PutAppendArgs* request,
                 ::raftKVRpcProctoc::PutAppendReply* response, ::google::protobuf::Closure* done) override;
  void Get(google::protobuf::RpcController* controller, const ::raftKVRpcProctoc::GetArgs* request,
           ::raftKVRpcProctoc::GetReply* response, ::google::protobuf::Closure* done) override;
  void FetchShardData(google::protobuf::RpcController* controller,
                      const ::raftKVRpcProctoc::FetchShardDataArgs* request,
                      ::raftKVRpcProctoc::FetchShardDataReply* response,
                      ::google::protobuf::Closure* done) override;

 private:
  int GetShardIdForKey(const std::string& key) const;
  bool IsShardServedByThisGroup(int shardId) const;
  bool IsShardReady(int shardId);
  bool IsShardReadyLocked(int shardId);
  void ApplyTopologyUpdate(const StaticGroupTopology& topology);
  bool TryFetchShardData(int shardId, int sourceGroupId, int configVersion, std::string* shardData,
                         std::unordered_map<std::string, int>* lastRequestId);
  ShardState& EnsureShardState(int shardId);
  const ShardState* FindShardState(int shardId) const;
  std::string SerializeLastRequestIdMap(const std::unordered_map<std::string, int>& requestMap) const;
  std::unordered_map<std::string, int> DeserializeLastRequestIdMap(const std::string& serialized) const;

  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& m_serializedShardData;
    ar& m_lastRequestIdByShard;
  }

  std::string getSnapshotData();
  void parseFromString(const std::string& str);
};

#endif  // SKIP_LIST_ON_RAFT_KVSERVER_H
