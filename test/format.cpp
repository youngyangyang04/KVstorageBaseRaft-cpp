#include "../src/common/include/util.h"

void myAssert(bool condition, std::string message) {
  if (!condition) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

int main() {
    // 测试格式化函数
    myAssert(false, format("[func-AppendEntries-rf{%d}] 两节点logIndex{%d}和term{%d}相同，但是其command{%d:%d}   "
                                 " {%d:%d}却不同！！\n",
                                 1, 2, 3, 4, 5, 6, 7));
    return 0;
}

// 编译命令: g++ test.cpp -o test -lboost_serialization


