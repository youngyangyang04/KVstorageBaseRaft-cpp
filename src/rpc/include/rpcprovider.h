#pragma once
#include <google/protobuf/descriptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <functional>
#include <string>
#include <unordered_map>
#include "google/protobuf/service.h"

// 框架提供的专门发布rpc服务的网络对象类
// todo:现在rpc客户端变成了 长连接，因此rpc服务器这边最好提供一个定时器，用以断开很久没有请求的连接。
// todo：为了配合这个，那么rpc客户端那边每次发送之前也需要真正的
class RpcProvider {
 public:
  // 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
  void NotifyService(google::protobuf::Service *service);

  // 启动rpc服务节点，开始提供rpc远程网络调用服务
  void Run(int nodeIndex, short port);

 private:
  // 组合EventLoop
  muduo::net::EventLoop m_eventLoop;
  std::shared_ptr<muduo::net::TcpServer> m_muduo_server;

  // service服务类型信息
  struct ServiceInfo {
    google::protobuf::Service *m_service;                                                     // 保存服务对象
    std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap;  // 保存服务方法
  };
  // 存储注册成功的服务对象和其服务方法的所有信息
  std::unordered_map<std::string, ServiceInfo> m_serviceMap;

  // 新的socket连接回调
  void OnConnection(const muduo::net::TcpConnectionPtr &);
  // 已建立连接用户的读写事件回调
  void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);
  // Closure的回调操作，用于序列化rpc的响应和网络发送
  void SendRpcResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);

 public:
  ~RpcProvider();
};