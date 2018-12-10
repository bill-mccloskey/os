#ifndef lazy_global_h
#define lazy_global_h

#include "assertions.h"
#include <new>

template<class T>
class LazyGlobal {
private:
  bool initialized_ = false;
  char repr_[sizeof(T)] __attribute__((aligned(8)));

public:
  template<class... Args>
  T& emplace(Args&&... args) {
    if (initialized_) {
      reinterpret_cast<T*>(repr_)->~T();
    }

    initialized_ = true;
    return *new(repr_) T(args...);
  }

  const T* operator->() const {
    assert(initialized_);
    return reinterpret_cast<const T*>(repr_);
  }

  T* operator-> () {
    assert(initialized_);
    return reinterpret_cast<T*>(repr_);
  }

  T& operator*() & {
    assert(initialized_);
    return *reinterpret_cast<T*>(repr_);
  }

  const T&& operator*() const && {
    assert(initialized_);
    return *reinterpret_cast<const T*>(repr_);
  }

  T&& operator*() && {
    assert(initialized_);
    return *reinterpret_cast<T*>(repr_);
  }

  explicit operator bool() const noexcept {
    return initialized_;
  }

  bool has_value() const noexcept {
    return initialized_;
  }

  T& value() & {
    assert(initialized_);
    return *reinterpret_cast<T*>(repr_);
  }

  const T& value() const & {
    assert(initialized_);
    return *reinterpret_cast<const T*>(repr_);
  }

  T&& value() && {
    assert(initialized_);
    return *reinterpret_cast<T*>(repr_);
  }

  const T&& value() const && {
    assert(initialized_);
    return *reinterpret_cast<const T*>(repr_);
  }
};

#endif
