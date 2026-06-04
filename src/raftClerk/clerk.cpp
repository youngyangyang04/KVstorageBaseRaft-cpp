//
// Created by swx on 23-6-4.
//
#include "clerk.h"

#include <string>
#include <vector>

#include "raftServerRpcUtil.h"
#include "util.h"

int Clerk::ResolveGroupId(const std::string& key) const {
  if (m_shardOwners.empty()) {
    return 0;
  }
  const int shardId = KeyToShard(key, m_shardCount);
  if (shardId < 0 || shardId >= static_cast<int>(m_shardOwners.size())) {
    return 0;
  }
  return m_shardOwners[shardId];
}

void Clerk::LoadRoutingConfig() {
  MprpcConfig config;
  config.LoadConfigFile(m_configFileName.c_str());
  const auto topology = LoadStaticGroupTopology(config);

  m_shardCount = topology.shardCount;
  m_shardOwners = topology.shardOwners;
  m_groupServers.clear();

  for (const auto& [groupId, servers] : topology.groupServers) {
    std::vector<std::shared_ptr<raftServerRpcUtil>> groupClients;
    groupClients.reserve(servers.size());
    for (const auto& [ip, port] : servers) {
      groupClients.push_back(std::shared_ptr<raftServerRpcUtil>(new raftServerRpcUtil(ip, port)));
    }
    m_groupServers[groupId] = std::move(groupClients);
    if (m_recentLeaderByGroup.find(groupId) == m_recentLeaderByGroup.end()) {
      m_recentLeaderByGroup[groupId] = 0;
    } else if (!m_groupServers[groupId].empty()) {
      m_recentLeaderByGroup[groupId] %= static_cast<int>(m_groupServers[groupId].size());
    }
  }
}

std::string Clerk::Get(std::string key) {
  m_requestId++;
  const auto requestId = m_requestId;

  while (true) {
    const int groupId = ResolveGroupId(key);
    auto serverIt = m_groupServers.find(groupId);
    if (serverIt == m_groupServers.end() || serverIt->second.empty()) {
      LoadRoutingConfig();
      continue;
    }

    auto& groupServers = serverIt->second;
    int server = m_recentLeaderByGroup[groupId] % static_cast<int>(groupServers.size());

    raftKVRpcProctoc::GetArgs args;
    args.set_key(key);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);

    for (int attempt = 0; attempt < static_cast<int>(groupServers.size()); ++attempt) {
      raftKVRpcProctoc::GetReply reply;
      bool ok = groupServers[server]->Get(&args, &reply);
      if (!ok || reply.err() == ErrWrongLeader) {
        server = (server + 1) % groupServers.size();
        continue;
      }
      if (reply.err() == ErrShardNotReady) {
        sleepNMilliseconds(200);
        continue;
      }
      if (reply.err() == ErrWrongGroup) {
        LoadRoutingConfig();
        break;
      }
      if (reply.err() == ErrNoKey) {
        m_recentLeaderByGroup[groupId] = server;
        return "";
      }
      if (reply.err() == OK) {
        m_recentLeaderByGroup[groupId] = server;
        return reply.value();
      }
    }
  }
}

void Clerk::PutAppend(std::string key, std::string value, std::string op) {
  m_requestId++;
  const auto requestId = m_requestId;

  while (true) {
    const int groupId = ResolveGroupId(key);
    auto serverIt = m_groupServers.find(groupId);
    if (serverIt == m_groupServers.end() || serverIt->second.empty()) {
      LoadRoutingConfig();
      continue;
    }

    auto& groupServers = serverIt->second;
    int server = m_recentLeaderByGroup[groupId] % static_cast<int>(groupServers.size());

    raftKVRpcProctoc::PutAppendArgs args;
    args.set_key(key);
    args.set_value(value);
    args.set_op(op);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);

    for (int attempt = 0; attempt < static_cast<int>(groupServers.size()); ++attempt) {
      raftKVRpcProctoc::PutAppendReply reply;
      bool ok = groupServers[server]->PutAppend(&args, &reply);
      if (!ok || reply.err() == ErrWrongLeader) {
        server = (server + 1) % groupServers.size();
        continue;
      }
      if (reply.err() == ErrShardNotReady) {
        sleepNMilliseconds(200);
        continue;
      }
      if (reply.err() == ErrWrongGroup) {
        LoadRoutingConfig();
        break;
      }
      if (reply.err() == OK) {
        m_recentLeaderByGroup[groupId] = server;
        return;
      }
    }
  }
}

void Clerk::Put(std::string key, std::string value) { PutAppend(key, value, "Put"); }

void Clerk::Append(std::string key, std::string value) { PutAppend(key, value, "Append"); }

void Clerk::Init(std::string configFileName) {
  m_configFileName = std::move(configFileName);
  LoadRoutingConfig();
}

Clerk::Clerk() : m_clientId(Uuid()), m_requestId(0), m_shardCount(kDefaultShardCount) {}
