//
// Created by henry on 24-1-23.
//

#ifndef KVRAFTCPP_DEFER_H
#define KVRAFTCPP_DEFER_H
#include <functional>
#include <utility>

template <class F>
class Defer {
 public:
  Defer(F&& f) : m_func(std::forward<F>(f)) {}
  Defer(const F& f) : m_func(f) {}
  ~Defer() { m_func(); }

  Defer(const Defer& e) = delete;
  Defer& operator=(const Defer& e) = delete;

 private:
  F m_func;
};

#define _CONCAT(a, b) a##b
#define _MAKE_DEFER_(line) Defer _CONCAT(defer, line) = [&]()

#undef DEFER
#define DEFER _MAKE_DEFER_(__LINE__)

#endif  // KVRAFTCPP_DEFER_H
