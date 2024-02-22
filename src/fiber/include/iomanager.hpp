#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "fcntl.h"
#include "scheduler.hpp"
#include "string.h"
#include "sys/epoll.h"
#include "timer.hpp"

namespace monsoon {
enum Event {
  NONE = 0x0,
  READ = 0x1,
  WRITE = 0x4,
};

struct EventContext {
  Scheduler *scheduler = nullptr;
  Fiber::ptr fiber;
  std::function<void()> cb;
};

class FdContext {
  friend class IOManager;

 public:
  // 获取事件上下文
  EventContext &getEveContext(Event event);
  // 重置事件上下文
  void resetEveContext(EventContext &ctx);
  // 触发事件
  void triggerEvent(Event event);

 private:
  EventContext read;
  EventContext write;
  int fd = 0;
  Event events = NONE;
  Mutex mutex;
};

class IOManager : public Scheduler, public TimerManager {
 public:
  typedef std::shared_ptr<IOManager> ptr;

  IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");
  ~IOManager();
  // 添加事件
  int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
  // 删除事件
  bool delEvent(int fd, Event event);
  // 取消事件
  bool cancelEvent(int fd, Event event);
  // 取消所有事件
  bool cancelAll(int fd);
  static IOManager *GetThis();

 protected:
  // 通知调度器有任务要调度
  void tickle() override;
  // 判断是否可以停止
  bool stopping() override;
  // idle协程
  void idle() override;
  // 判断是否可以停止，同时获取最近一个定时超时时间
  bool stopping(uint64_t &timeout);

  void OnTimerInsertedAtFront() override;
  void contextResize(size_t size);

 private:
  int epfd_ = 0;
  int tickleFds_[2];
  // 正在等待执行的IO事件数量
  std::atomic<size_t> pendingEventCnt_ = {0};
  RWMutex mutex_;
  std::vector<FdContext *> fdContexts_;
};
}  // namespace monsoon

#endif