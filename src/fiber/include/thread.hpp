#ifndef __SYLAR_THREAD_H_
#define __SYLAR_THREAD_H_

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

namespace monsoon {
class Thread {
 public:
  typedef std::shared_ptr<Thread> ptr;
  Thread(std::function<void()> cb, const std::string &name);
  ~Thread();
  pid_t getId() const { return id_; }
  const std::string &getName() const { return name_; }
  void join();
  static Thread *GetThis();
  static const std::string &GetName();
  static void SetName(const std::string &name);

 private:
  Thread(const Thread &) = delete;
  Thread(const Thread &&) = delete;
  Thread operator=(const Thread &) = delete;

  static void *run(void *args);

 private:
  pid_t id_;
  pthread_t thread_;
  std::function<void()> cb_;
  std::string name_;
};
}  // namespace monsoon

#endif