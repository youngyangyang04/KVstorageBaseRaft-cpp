//
// Created by swx on 23-12-21.
//
#include <iostream>

// #include "mprpcapplication.h"
#include "rpcExample/friend.pb.h"

#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include "rpcprovider.h"

int main(int argc, char **argv) {
  // https://askubuntu.com/questions/754213/what-is-difference-between-localhost-address-127-0-0-1-and-127-0-1-1
  std::string ip = "127.0.1.1";
  short port = 7788;

  // 演示调用远程发布的rpc方法Login
  fixbug::FiendServiceRpc_Stub stub(
      new MprpcChannel(ip, port, true));  //注册进自己写的channel类，channel类用于自定义发送格式和负责序列化等操作
  // rpc方法的请求参数
  fixbug::GetFriendsListRequest request;
  request.set_userid(1000);
  // rpc方法的响应
  fixbug::GetFriendsListResponse response;
  // 发起rpc方法的调用,消费这的stub最后都会调用到channel的 call_method方法  同步的rpc调用过程  MprpcChannel::callmethod
  MprpcController controller;
  //長連接測試 ，發送10次請求
  int count = 10;
  while (count--) {
    std::cout << " 倒数" << count << "次发起RPC请求" << std::endl;
    stub.GetFriendsList(&controller, &request, &response, nullptr);
    // RpcChannel->RpcChannel::callMethod 集中来做所有rpc方法调用的参数序列化和网络发送

    // 一次rpc调用完成，读调用的结果
    // rpc调用是否失败由框架来决定（rpc调用失败 ！= 业务逻辑返回false）
    // rpc和业务本质上是隔离的
    if (controller.Failed()) {
      std::cout << controller.ErrorText() << std::endl;
    } else {
      if (0 == response.result().errcode()) {
        std::cout << "rpc GetFriendsList response success!" << std::endl;
        int size = response.friends_size();
        for (int i = 0; i < size; i++) {
          std::cout << "index:" << (i + 1) << " name:" << response.friends(i) << std::endl;
        }
      } else {
        //这里不是rpc失败，
        // 而是业务逻辑的返回值是失败
        // 两者要区分清楚
        std::cout << "rpc GetFriendsList response error : " << response.result().errmsg() << std::endl;
      }
    }
    sleep(5);  // sleep 5 seconds
  }
  return 0;
}
