#pragma once

#include <climits>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mprpcconfig.h"

constexpr int kDefaultShardCount = 16;

inline int ParseConfigInt(const std::string& rawValue, int defaultValue) {
  if (rawValue.empty()) {
    return defaultValue;
  }
  return std::atoi(rawValue.c_str());
}

inline std::vector<int> ParseShardList(const std::string& rawValue) {
  std::vector<int> shards;
  if (rawValue.empty()) {
    return shards;
  }

  std::string token;
  for (char ch : rawValue) {
    if (ch == ',') {
      if (!token.empty()) {
        shards.push_back(std::atoi(token.c_str()));
        token.clear();
      }
      continue;
    }
    if (ch != ' ') {
      token.push_back(ch);
    }
  }

  if (!token.empty()) {
    shards.push_back(std::atoi(token.c_str()));
  }

  return shards;
}

inline int KeyToShard(const std::string& key, int shardCount) {
  if (shardCount <= 0) {
    return 0;
  }

  return static_cast<int>(std::hash<std::string>{}(key) % static_cast<size_t>(shardCount));
}

inline std::string GroupNodePrefix(int groupId) { return "group" + std::to_string(groupId) + "_node"; }

inline std::vector<std::pair<std::string, short>> LoadNodeList(MprpcConfig& config, const std::string& prefix) {
  std::vector<std::pair<std::string, short>> nodes;
  for (int i = 0; i < INT_MAX - 1; ++i) {
    const std::string nodeKey = prefix + std::to_string(i);
    const std::string ip = config.Load(nodeKey + "ip");
    const std::string port = config.Load(nodeKey + "port");
    if (ip.empty() || port.empty()) {
      break;
    }
    nodes.emplace_back(ip, static_cast<short>(std::atoi(port.c_str())));
  }
  return nodes;
}

struct StaticGroupTopology {
  int shardCount = kDefaultShardCount;
  int groupCount = 1;
  std::vector<int> shardOwners;
  std::unordered_map<int, std::vector<std::pair<std::string, short>>> groupServers;
};

inline StaticGroupTopology LoadStaticGroupTopology(MprpcConfig& config) {
  StaticGroupTopology topology;
  topology.shardCount =
      ParseConfigInt(config.Load("cluster_shard_count"), ParseConfigInt(config.Load("shard_count"), kDefaultShardCount));

  int configuredGroupCount =
      ParseConfigInt(config.Load("cluster_group_count"), ParseConfigInt(config.Load("group_count"), 0));
  if (configuredGroupCount <= 0) {
    configuredGroupCount = 0;
    while (true) {
      const std::string probeIp = config.Load(GroupNodePrefix(configuredGroupCount) + "0ip");
      if (probeIp.empty()) {
        break;
      }
      ++configuredGroupCount;
    }
  }
  topology.groupCount = configuredGroupCount > 0 ? configuredGroupCount : 1;

  for (int groupId = 0; groupId < topology.groupCount; ++groupId) {
    auto nodes = LoadNodeList(config, GroupNodePrefix(groupId));
    if (groupId == 0 && nodes.empty()) {
      nodes = LoadNodeList(config, "node");
    }
    topology.groupServers.emplace(groupId, std::move(nodes));
  }

  topology.shardOwners.assign(topology.shardCount, 0);
  bool explicitAssignment = false;
  for (int groupId = 0; groupId < topology.groupCount; ++groupId) {
    const auto shards = ParseShardList(config.Load("group" + std::to_string(groupId) + "_shards"));
    if (shards.empty()) {
      continue;
    }
    explicitAssignment = true;
    for (int shardId : shards) {
      if (shardId >= 0 && shardId < topology.shardCount) {
        topology.shardOwners[shardId] = groupId;
      }
    }
  }

  if (!explicitAssignment && topology.groupCount > 1) {
    for (int shardId = 0; shardId < topology.shardCount; ++shardId) {
      topology.shardOwners[shardId] = shardId % topology.groupCount;
    }
  }

  return topology;
}
