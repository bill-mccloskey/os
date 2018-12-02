#ifndef refcount_h
#define refcount_h

#include "types.h"

class RefCounted {
public:
  void IncRef() { refcount_++; }
  int DecRef() { return --refcount_; }

private:
  int64_t refcount_ = 0;
};

template<typename T>
class RefPtr {
public:
  RefPtr() {}
  RefPtr(T* p) : ref_(p) {
    if (ref_) ref_->IncRef();
  }
  RefPtr(const RefPtr& r) : ref_(r.ref_) {
    if (ref_) ref_->IncRef();
  }
  RefPtr(RefPtr&& r) : ref_(r.ref_) {
    r.ref_ = nullptr;
  }

  ~RefPtr() {
    reset();
  }

  T& operator*() const {
    return *ref_;
  }
  T* operator->() const {
    return ref_;
  }
  operator bool() const {
    return ref_ != nullptr;
  }
  T* value() const {
    return ref_;
  }

  void reset() {
    if (!ref_) return;
    int64_t rc = ref_->DecRef();
    if (rc == 0) delete ref_;
    ref_ = nullptr;
  }

  RefPtr& operator=(const RefPtr& r) {
    reset();
    ref_ = r.ref_;
    if (ref_) ref_->IncRef();
  }

  RefPtr& operator=(RefPtr&& r) {
    reset();
    ref_ = r.ref_;
    r.ref_ = nullptr;
  }

private:
  T* ref_ = nullptr;
};

#endif
