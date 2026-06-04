//
// Created by swx on 23-12-28.
//
#include <iostream>
#include <memory>
#include <cstdlib>
#include <unistd.h>

#include "kvServer.h"
#include "mprpcconfig.h"
#include "shardConfig.h"

namespace {
short ResolveNodePort(MprpcConfig& config, int groupId, int nodeId) {
  auto topology = LoadStaticGroupTopology(config);
  auto it = topology.groupServers.find(groupId);
  if (it == topology.groupServers.end() || nodeId < 0 || nodeId >= static_cast<int>(it->second.size())) {
    return 0;
  }
  return it->second[nodeId].second;
}
}  // namespace

void ShowArgsHelp();

int main(int argc, char** argv) {
  if (argc < 2) {
    ShowArgsHelp();
    exit(EXIT_FAILURE);
  }

  int c = 0;
  int nodeId = 0;
  int groupId = 0;
  std::string configFileName;

  while ((c = getopt(argc, argv, "g:n:f:")) != -1) {
    switch (c) {
      case 'g':
        groupId = atoi(optarg);
        break;
      case 'n':
        nodeId = atoi(optarg);
        break;
      case 'f':
        configFileName = optarg;
        break;
      default:
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }
  }

  MprpcConfig config;
  config.LoadConfigFile(configFileName.c_str());
  short port = ResolveNodePort(config, groupId, nodeId);
  if (port == 0) {
    std::cout << "Error: could not resolve port for group " << groupId << " node " << nodeId << " from "
              << configFileName << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "Starting shard-aware raftkv node: group=" << groupId << " node=" << nodeId << " port=" << port
            << " pid=" << getpid() << std::endl;

  auto kvServer = std::make_unique<KvServer>(nodeId, groupId, 500, configFileName, port);
  pause();
  return 0;
}

void ShowArgsHelp() { std::cout << "format: command -g <groupId> -n <nodeId> -f <configFileName>" << std::endl; }
