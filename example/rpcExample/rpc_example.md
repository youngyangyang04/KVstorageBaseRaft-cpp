
# rpc_example



1.库准备
- proctoc

2.编写自己想要发送的rpc实例
参考`friend.proto`文件即可

3.生成对应的pb.h和pb.cc文件
```
protoc friend.proto --cpp_out=.
```

4.编写rpc客户端和服务端

代码可参考`friendServer.cpp`和`callFriendService.cpp`文件。