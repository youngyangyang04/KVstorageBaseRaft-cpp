

add_subdirectory(skipList)

add_subdirectory(common) #需要注意如果rpc中需要依靠common中的代码，则需要在rpc在前，其cmake文件中的set才对rpc文件夹可见
# 使用include_directories()函数应该也是可行的
add_subdirectory(rpc)

add_subdirectory(fiber)

add_subdirectory(raftRpcPro)

add_subdirectory(raftCore)

add_subdirectory(raftClerk)

