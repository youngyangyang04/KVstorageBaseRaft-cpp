//
// Created by swx on 24-1-5.
//
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

class SerializablePair {
 public:
  SerializablePair() = default;
  SerializablePair(const std::string& first, const std::string& second) : first(first), second(second) {}

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& first;
    ar& second;
  }

 private:
  std::string first;
  std::string second;
};

int main() {
  // 创建 vector<pair<string, string>> 对象
  std::vector<std::pair<std::string, std::string>> data;
  data.emplace_back("key1", "value1");
  data.emplace_back("key2", "value2");
  data.emplace_back("key3", "value3");

  // 打开一个输出文件流
  std::ofstream ofs("data_vector.txt");

  // 创建文本输出存档对象
  boost::archive::text_oarchive oa(ofs);

  // 序列化 vector<pair<string, string>> 到文本输出存档
  oa << data;

  // 关闭输出文件流
  ofs.close();

  // 打开一个输入文件流
  std::ifstream ifs("data_vector.txt");

  // 创建文本输入存档对象
  boost::archive::text_iarchive ia(ifs);

  // 创建空的 vector<pair<string, string>> 对象
  std::vector<std::pair<std::string, std::string>> loadedData;

  // 反序列化数据到 vector<pair<string, string>> 对象
  ia >> loadedData;

  // 输出反序列化后的数据
  for (const auto& pair : loadedData) {
    std::cout << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
  }

  return 0;
}
