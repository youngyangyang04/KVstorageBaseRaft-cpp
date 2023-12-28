//
// Created by swx on 23-12-28.
//

#include "raftRpc.h"

#include <mprpcchannel.h>
#include <mprpccontroller.h>


bool RaftRpc::AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response)
 {

     MprpcController controller;
     stub_->AppendEntries(&controller, args, response, nullptr);
     return !controller.Failed();
 }

bool RaftRpc::InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args, raftRpcProctoc::InstallSnapshotResponse *response)
 {
     MprpcController controller;
     stub_->InstallSnapshot(&controller, args, response, nullptr);
     return !controller.Failed();
 }

bool RaftRpc::RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response)
 {
     MprpcController controller;
     stub_->RequestVote(&controller, args, response, nullptr);
     return !controller.Failed();
 }
//先开启服务器，再尝试连接其他的节点，中间给一个间隔时间，等待其他的rpc服务器节点启动

RaftRpc::RaftRpc(std::string ip,short port)
 {
     //*********************************************  */
     //发送rpc设置
     stub_ = new raftRpcProctoc::raftRpc_Stub(new MprpcChannel(ip, port,true));
 }

RaftRpc::~RaftRpc()
 {
     delete stub_;
 }
