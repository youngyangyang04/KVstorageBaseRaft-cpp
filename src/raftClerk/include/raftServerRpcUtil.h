#ifndef RAFTSERVERRPC_H
#define RAFTSERVERRPC_H

#include <iostream>

#include "kvServerRPC.pb.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include "rpcprovider.h"

class raftServerRpcUtil {
 private:
  raftKVRpcProctoc::kvServerRpc_Stub* stub;

 public:
  bool Get(raftKVRpcProctoc::GetArgs* getArgs, raftKVRpcProctoc::GetReply* reply);
  bool PutAppend(raftKVRpcProctoc::PutAppendArgs* args, raftKVRpcProctoc::PutAppendReply* reply);
  bool FetchShardData(raftKVRpcProctoc::FetchShardDataArgs* args, raftKVRpcProctoc::FetchShardDataReply* reply);

  raftServerRpcUtil(std::string ip, short port);
  ~raftServerRpcUtil();
};

#endif  // RAFTSERVERRPC_H
