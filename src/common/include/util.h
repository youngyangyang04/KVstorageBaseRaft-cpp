#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "config.h"

template <class F>
class DeferClass {
 public:
  DeferClass(F&& f) : m_func(std::forward<F>(f)) {}
  DeferClass(const F& f) : m_func(f) {}
  ~DeferClass() { m_func(); }

  DeferClass(const DeferClass& e) = delete;
  DeferClass& operator=(const DeferClass& e) = delete;

 private:
  F m_func;
};

#define _CONCAT(a, b) a##b
#define _MAKE_DEFER_(line) DeferClass _CONCAT(defer_placeholder, line) = [&]()

#undef DEFER
#define DEFER _MAKE_DEFER_(__LINE__)

void DPrintf(const char* format, ...);

void myAssert(bool condition, std::string message = "Assertion failed!");

template <typename... Args>
std::string format(const char* format_str, Args... args) {
  int size_s = std::snprintf(nullptr, 0, format_str, args...) + 1;
  if (size_s <= 0) {
    throw std::runtime_error("Error during formatting.");
  }
  auto size = static_cast<size_t>(size_s);
  std::vector<char> buf(size);
  std::snprintf(buf.data(), size, format_str, args...);
  return std::string(buf.data(), buf.data() + size - 1);
}

std::chrono::_V2::system_clock::time_point now();

std::chrono::milliseconds getRandomizedElectionTimeout();
void sleepNMilliseconds(int N);

template <typename T>
class LockQueue {
 public:
  void Push(const T& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(data);
    m_condvariable.notify_one();
  }

  T Pop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.empty()) {
      m_condvariable.wait(lock);
    }
    T data = m_queue.front();
    m_queue.pop();
    return data;
  }

  bool timeOutPop(int timeout, T* resData) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    auto timeoutTime = now + std::chrono::milliseconds(timeout);

    while (m_queue.empty()) {
      if (m_condvariable.wait_until(lock, timeoutTime) == std::cv_status::timeout) {
        return false;
      }
    }

    T data = m_queue.front();
    m_queue.pop();
    *resData = data;
    return true;
  }

 private:
  std::queue<T> m_queue;
  std::mutex m_mutex;
  std::condition_variable m_condvariable;
};

class Op {
 public:
  std::string Operation;
  std::string Key;
  std::string Value;
  std::string ClientId;
  int RequestId = 0;
  int ConfigVersion = 0;
  int ShardId = -1;
  std::string ShardData;
  std::unordered_map<std::string, int> ShardLastRequestId;

 public:
  std::string asString() const {
    std::stringstream ss;
    boost::archive::text_oarchive oa(ss);
    oa << *this;
    return ss.str();
  }

  bool parseFromString(std::string str) {
    std::stringstream iss(str);
    boost::archive::text_iarchive ia(iss);
    ia >> *this;
    return true;
  }

 public:
  friend std::ostream& operator<<(std::ostream& os, const Op& obj) {
    os << "[Op Operation{" << obj.Operation << "} Key{" << obj.Key << "} Value{" << obj.Value << "} ClientId{"
       << obj.ClientId << "} RequestId{" << obj.RequestId << "} ConfigVersion{" << obj.ConfigVersion
       << "} ShardId{" << obj.ShardId << "}]";
    return os;
  }

 private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& Operation;
    ar& Key;
    ar& Value;
    ar& ClientId;
    ar& RequestId;
    ar& ConfigVersion;
    ar& ShardId;
    ar& ShardData;
    ar& ShardLastRequestId;
  }
};

const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";
const std::string ErrWrongGroup = "ErrWrongGroup";
const std::string ErrShardNotReady = "ErrShardNotReady";

bool isReleasePort(unsigned short usPort);

bool getReleasePort(short& port);

#endif  // UTIL_H
