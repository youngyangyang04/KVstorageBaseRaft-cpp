#include "utils.hpp"

namespace monsoon {
pid_t GetThreadId() { return syscall(SYS_gettid); }

u_int32_t GetFiberId() {
  // TODO
  return 0;
}
}  // namespace monsoon