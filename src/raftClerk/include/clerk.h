//
// Created by swx on 23-6-4.
//

#ifndef SKIP_LIST_ON_RAFT_CLERK_H
#define SKIP_LIST_ON_RAFT_CLERK_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <atomic>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "kvServerRPC.pb.h"
#include "mprpcconfig.h"
#include "raftServerRpcUtil.h"
#include "shardConfig.h"

class Clerk {
 private:
  std::unordered_map<int, std::vector<std::shared_ptr<raftServerRpcUtil>>> m_groupServers;
  std::unordered_map<int, int> m_recentLeaderByGroup;
  std::vector<int> m_shardOwners;
  std::string m_clientId;
  std::string m_configFileName;
  int m_requestId;
  int m_shardCount;

  std::string Uuid() {
    static std::atomic<unsigned long long> counter{0};
    std::stringstream ss;
    ss << std::chrono::steady_clock::now().time_since_epoch().count() << "-" << getpid() << "-"
       << std::hash<std::thread::id>{}(std::this_thread::get_id()) << "-" << counter.fetch_add(1);
    return ss.str();
  }

  int ResolveGroupId(const std::string& key) const;
  void LoadRoutingConfig();
  void PutAppend(std::string key, std::string value, std::string op);

 public:
  void Init(std::string configFileName);
  std::string Get(std::string key);

  void Put(std::string key, std::string value);
  void Append(std::string key, std::string value);

 public:
  Clerk();
};

#endif  // SKIP_LIST_ON_RAFT_CLERK_H
