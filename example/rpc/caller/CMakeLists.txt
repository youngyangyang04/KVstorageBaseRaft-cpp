# set(SRC_LIST calluserservice.cc ../user.pb.cc)
set(SRC_LIST callFriendService.cpp ../friend.pb.cc)
add_executable(consumer ${SRC_LIST})
target_link_libraries(consumer rpc_lib protobuf)