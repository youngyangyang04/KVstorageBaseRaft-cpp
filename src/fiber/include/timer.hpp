#ifndef __MONSOON_TIMER_H__
#  define __MONSSON_TIMER_H__

#  include <memory>
#  include <set>
#  include <vector>
#  include "mutex.hpp"

namespace monsoon {
class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;

 public:
  typedef std::shared_ptr<Timer> ptr;

  bool cancel();
  bool refresh();
  bool reset(uint64_t ms, bool from_now);

 private:
  Timer(uint64_t ms, std::function<void()> cb, bool recuring, TimerManager *manager);
  Timer(uint64_t next);

  // 是否是循环定时器
  bool recurring_ = false;
  // 执行周期
  uint64_t ms_ = 0;
  // 精确的执行时间
  uint64_t next_ = 0;
  // 回调函数
  std::function<void()> cb_;
  // 管理器
  TimerManager *manager_ = nullptr;

 private:
  struct Comparator {
    bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
  };
};

class TimerManager {
  friend class Timer;

 public:
  TimerManager();
  virtual ~TimerManager();
  Timer::ptr addTimer(uint64_t ms, std::function<void()> cb, bool recuring = false);
  Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond,
                               bool recurring = false);
  // 到最近一个定时器的时间间隔（ms）
  uint64_t getNextTimer();
  // 获取需要执行的定时器的回调函数列表
  void listExpiredCb(std::vector<std::function<void()>> &cbs);
  // 是否有定时器
  bool hasTimer();

 protected:
  // 当有新的定时器插入到定时器首部，执行该函数
  virtual void OnTimerInsertedAtFront() = 0;
  // 将定时器添加到管理器
  void addTimer(Timer::ptr val, RWMutex::WriteLock &lock);

 private:
  // 检测服务器时间是否被调后了
  bool detectClockRolllover(uint64_t now_ms);

  RWMutex mutex_;
  // 定时器集合
  std::set<Timer::ptr, Timer::Comparator> timers_;
  // 是否触发OnTimerInsertedAtFront
  bool tickled_ = false;
  // 上次执行时间
  uint64_t previouseTime_ = 0;
};
}  // namespace monsoon

#endif