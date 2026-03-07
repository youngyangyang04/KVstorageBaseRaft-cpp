//
// Created by swx on 23-12-28.
//
#include <iostream>
#include "raft.h"
#include "mprpcconfig.h"
// #include "kvServer.h"
#include <kvServer.h>
#include <unistd.h>
#include <iostream>
#include <random>

void ShowArgsHelp();

int main(int argc, char **argv) {
  if (argc < 2) {
    ShowArgsHelp();
    exit(EXIT_FAILURE);
  }
  int c = 0;
  int nodeId = 0; // 修改为单节点 ID
  std::string configFileName;
  
  while ((c = getopt(argc, argv, "n:f:")) != -1) {
    switch (c) {
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

  // 1. 获取当前节点应该监听的端口 (从 test.conf 中解析)
  // 为了简单起见，我们直接传递命令行参数指定的端口，或者在 KvServer 内部解析
  // 这里我们假设端口已经在 test.conf 中由 RpcProvider::Run 处理或手动配置
  // 注意：原代码逻辑比较混乱，我们将其简化为运行一个指定的节点
  
  // 我们从 test.conf 中读取属于该 nodeID 的端口
  MprpcConfig config;
  config.LoadConfigFile(configFileName.c_str());
  std::string portKey = "node" + std::to_string(nodeId) + "port";
  short port = atoi(config.Load(portKey).c_str());

  if (port == 0) {
      std::cout << "Error: Could not find port for node " << nodeId << " in " << configFileName << std::endl;
      exit(EXIT_FAILURE);
  }

  std::cout << "Starting raftkv node:" << nodeId << " on port:" << port << " pid:" << getpid() << std::endl;
  
  auto kvServer = new KvServer(nodeId, 500, configFileName, port);
  pause(); 

  return 0;
}

void ShowArgsHelp() { std::cout << "format: command -n <nodeNum> -f <configFileName>" << std::endl; }
