#include "thread.hpp"
#include "utils.hpp"

namespace monsoon {
// 指向当前线程
static thread_local Thread *cur_thread = nullptr;
static thread_local std::string cur_thread_name = "UNKNOW";

Thread::Thread(std::function<void()> cb, const std::string &name = "UNKNOW") : cb_(cb), name_(name) {
  if (name.empty()) {
    name_ = "UNKNOW";
  }

  int rt = pthread_create(&thread_, nullptr, &Thread::run, this);
  if (rt) {
    std::cout << "pthread_create error,name:" << name_ << std::endl;
    throw std::logic_error("pthread_create");
  }
}

void *Thread::run(void *arg) {
  Thread *thread = (Thread *)arg;
  cur_thread = thread;
  cur_thread_name = thread->name_;
  thread->id_ = monsoon::GetThreadId();
  // 给线程命名
  pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());
  std::function<void()> cb;
  cb.swap(thread->cb_);
  // 启动回调函数
  // std::cout << "begin callback " << std::endl;
  cb();
  return 0;
}

Thread::~Thread() {
  if (thread_) {
    pthread_detach(thread_);
  }
}

void Thread::join() {
  if (thread_) {
    int rt = pthread_join(thread_, nullptr);
    if (rt) {
      std::cout << "pthread_join error,name:" << name_ << std::endl;
      throw std::logic_error("pthread_join");
    }
    thread_ = 0;
  }
}

Thread *Thread::GetThis() { return cur_thread; }

const std::string &Thread::GetName() { return cur_thread_name; }

void Thread::SetName(const std::string &name) {
  if (name.empty()) {
    return;
  }
  if (cur_thread) {
    cur_thread->name_ = name;
  }
  cur_thread_name = name;
}

}  // namespace monsoon
