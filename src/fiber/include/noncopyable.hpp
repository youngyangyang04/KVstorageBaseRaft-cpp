#ifndef __SYLAR_NONCOPYABLE_H__
#define __SYLAR_NONCOPYABLE_H__

namespace monsoon {
class Nonecopyable {
 public:
  Nonecopyable() = default;
  ~Nonecopyable() = default;
  Nonecopyable(const Nonecopyable &) = delete;
  Nonecopyable operator=(const Nonecopyable) = delete;
};
}  // namespace monsoon

#endif