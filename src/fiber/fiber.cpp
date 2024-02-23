#include "fiber.hpp"
#include <assert.h>
#include <atomic>
#include "scheduler.hpp"
#include "utils.hpp"

namespace monsoon {
const bool DEBUG = true;
// 当前线程正在运行的协程
static thread_local Fiber *cur_fiber = nullptr;
// 当前线程的主协程
static thread_local Fiber::ptr cur_thread_fiber = nullptr;
// 用于生成协程Id
static std::atomic<uint64_t> cur_fiber_id{0};
// 统计当前协程数
static std::atomic<uint64_t> fiber_count{0};
// 协议栈默认大小 128k
static int g_fiber_stack_size = 128 * 1024;

class StackAllocator {
 public:
  static void *Alloc(size_t size) { return malloc(size); }
  static void Delete(void *vp, size_t size) { return free(vp); }
};
// only for GetThis
Fiber::Fiber() {
  SetThis(this);
  state_ = RUNNING;
  CondPanic(getcontext(&ctx_) == 0, "getcontext error");
  ++fiber_count;
  id_ = cur_fiber_id++;
  std::cout << "[fiber] create fiber , id = " << id_ << std::endl;
  //",backtrace:\n"<< BacktraceToString(6, 3, "") << std::endl;
}

// 设置当前协程
void Fiber::SetThis(Fiber *f) { cur_fiber = f; }
// 获取当前执行协程，不存在则创建
Fiber::ptr Fiber::GetThis() {
  if (cur_fiber) {
    return cur_fiber->shared_from_this();
  }
  // 创建主协程并初始化
  Fiber::ptr main_fiber(new Fiber);
  CondPanic(cur_fiber == main_fiber.get(), "cur_fiber need to be main_fiber");
  cur_thread_fiber = main_fiber;
  return cur_fiber->shared_from_this();
}

// 有参构造，并为新的子协程创建栈空间
Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_inscheduler)
    : id_(cur_fiber_id++), cb_(cb), isRunInScheduler_(run_inscheduler) {
  ++fiber_count;
  stackSize_ = stacksize > 0 ? stacksize : g_fiber_stack_size;
  stack_ptr = StackAllocator::Alloc(stackSize_);
  CondPanic(getcontext(&ctx_) == 0, "getcontext error");
  // 初始化协程上下文
  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_ptr;
  ctx_.uc_stack.ss_size = stackSize_;
  makecontext(&ctx_, &Fiber::MainFunc, 0);

  // std::cout << "create son fiber , id = " << id_ << ",backtrace:\n"
  //           << BacktraceToString(6, 3, "") << std::endl;
  // std::cout << "[fiber]create son fiber , id = " << id_ << std::endl;
}

// 切换当前协程到执行态,并保存主协程的上下文
void Fiber::resume() {
  CondPanic(state_ != TERM && state_ != RUNNING, "state error");
  SetThis(this);
  state_ = RUNNING;

  if (isRunInScheduler_) {
    // 当前协程参与调度器调度，则与调度器主协程进行swap
    CondPanic(0 == swapcontext(&(Scheduler::GetMainFiber()->ctx_), &ctx_),
              "isRunInScheduler_ = true,swapcontext error");
  } else {
    // 切换主协程到当前协程，并保存主协程上下文到子协程ctx_
    CondPanic(0 == swapcontext(&(cur_thread_fiber->ctx_), &ctx_), "isRunInScheduler_ = false,swapcontext error");
  }
}

// 当前协程让出执行权
// 协程执行完成之后胡会自动yield,回到主协程，此时状态为TEAM
void Fiber::yield() {
  CondPanic(state_ == TERM || state_ == RUNNING, "state error");
  SetThis(cur_thread_fiber.get());
  if (state_ != TERM) {
    state_ = READY;
  }
  if (isRunInScheduler_) {
    CondPanic(0 == swapcontext(&ctx_, &(Scheduler::GetMainFiber()->ctx_)),
              "isRunInScheduler_ = true,swapcontext error");
  } else {
    // 切换当前协程到主协程，并保存子协程的上下文到主协程ctx_
    CondPanic(0 == swapcontext(&ctx_, &(cur_thread_fiber->ctx_)), "swapcontext failed");
  }
}

// 协程入口函数
void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis();
  CondPanic(cur != nullptr, "cur is nullptr");

  cur->cb_();
  cur->cb_ = nullptr;
  cur->state_ = TERM;
  // 手动使得cur_fiber引用计数减1
  auto raw_ptr = cur.get();
  cur.reset();
  // 协程结束，自动yield,回到主协程
  // 访问原始指针原因：reset后cur已经被释放
  raw_ptr->yield();
}

// 协程重置（复用已经结束的协程，复用其栈空间，创建新协程）
// TODO:暂时不允许Ready状态下的重置
void Fiber::reset(std::function<void()> cb) {
  CondPanic(stack_ptr, "stack is nullptr");
  CondPanic(state_ == TERM, "state isn't TERM");
  cb_ = cb;
  CondPanic(0 == getcontext(&ctx_), "getcontext failed");
  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_ptr;
  ctx_.uc_stack.ss_size = stackSize_;

  makecontext(&ctx_, &Fiber::MainFunc, 0);
  state_ = READY;
}

Fiber::~Fiber() {
  --fiber_count;
  if (stack_ptr) {
    // 有栈空间，说明是子协程
    CondPanic(state_ == TERM, "fiber state should be term");
    StackAllocator::Delete(stack_ptr, stackSize_);
    // std::cout << "dealloc stack,id = " << id_ << std::endl;
  } else {
    // 没有栈空间，说明是线程的主协程
    CondPanic(!cb_, "main fiber no callback");
    CondPanic(state_ == RUNNING, "main fiber state should be running");

    Fiber *cur = cur_fiber;
    if (cur == this) {
      SetThis(nullptr);
    }
  }
}

}  // namespace monsoon