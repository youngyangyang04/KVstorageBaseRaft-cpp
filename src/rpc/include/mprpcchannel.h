#ifndef MPRPCCHANNEL_H
#define MPRPCCHANNEL_H

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <algorithm>
#include <algorithm>  // 包含 std::generate_n() 和 std::generate() 函数的头文件
#include <functional>
#include <iostream>
#include <map>
#include <random>  // 包含 std::uniform_int_distribution 类型的头文件
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

// 真正负责发送和接受的前后处理工作
//  如消息的组织方式，向哪个节点发送等等
class MprpcChannel : public google::protobuf::RpcChannel {
 public:
  // 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送 那一步
  void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request, google::protobuf::Message *response,
                  google::protobuf::Closure *done) override;
  MprpcChannel(string ip, short port, bool connectNow);

 private:
  int m_clientFd;
  const std::string m_ip;  //保存ip和端口，如果断了可以尝试重连
  const uint16_t m_port;
  /// @brief 连接ip和端口,并设置m_clientFd
  /// @param ip ip地址，本机字节序
  /// @param port 端口，本机字节序
  /// @return 成功返回空字符串，否则返回失败信息
  bool newConnect(const char *ip, uint16_t port, string *errMsg);
};

#endif  // MPRPCCHANNEL_H