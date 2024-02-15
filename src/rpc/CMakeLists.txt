
# 获取当前源文件所在目录的绝对路径
get_filename_component(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

# 将该目录下所有源文件添加到 SRC_LIST 变量中
aux_source_directory(${SRC_DIR} SRC_LIST)

add_library(rpc_lib ${SRC_LIST} ${src_common} )
target_link_libraries(rpc_lib boost_serialization)
set(src_rpc ${SRC_LIST} CACHE INTERNAL "Description of the variable")

