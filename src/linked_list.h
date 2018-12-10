#ifndef linked_list_h
#define linked_list_h

#include "assertions.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

template<typename T, size_t>
class LinkedList;

class LinkedListEntry {
public:
  void Remove() {
    prev_->next_ = next_;
    next_->prev_ = prev_;
    next_ = prev_ = nullptr;
  }
  void InsertBefore(LinkedListEntry& entry) {
    assert(!entry.next_);
    assert(!entry.prev_);

    entry.prev_ = prev_;
    entry.next_ = this;

    prev_->next_ = &entry;
    prev_ = &entry;
  }
  void InsertAfter(LinkedListEntry& entry) {
    assert(!entry.next_);
    assert(!entry.prev_);

    entry.prev_ = this;
    entry.next_ = next_;

    next_->prev_ = &entry;
    next_ = &entry;
  }

  bool InList() const {
    assert((next_ == nullptr) == (prev_ == nullptr));
    return next_ != nullptr;
  }

private:
  template<typename T, size_t>
  friend class LinkedList;

  LinkedListEntry* next_ = nullptr;
  LinkedListEntry* prev_ = nullptr;
};

#define LINKED_LIST(T, entry_field) \
  LinkedList<T, offsetof(T, entry_field)>

template<typename T, size_t entry_offset>
class LinkedList {
public:
  LinkedList() {
    sentinel_.next_ = &sentinel_;
    sentinel_.prev_ = &sentinel_;
  }

  class Iter {
  public:
    T& operator*() const {
      assert_ne(entry_, term_);
      return *ElementFrom(entry_);
    }

    T* operator->() const {
      assert_ne(entry_, term_);
      return ElementFrom(entry_);
    }

    const Iter& operator++() {
      if (reverse_) entry_ = entry_->prev_;
      else entry_ = entry_->next_;
      return *this;
    }

    Iter operator++(int) {
      Iter result = *this;
      ++(*this);
      return result;
    }

    const Iter& operator--() {
      if (reverse_) entry_ = entry_->next_;
      else entry_ = entry_->prev_;
      return *this;
    }

    Iter operator--(int) {
      Iter result = *this;
      --(*this);
      return result;
    }

    bool operator!=(const Iter& other) const {
      return entry_ != other.entry_;
    }

    bool operator==(const Iter& other) const {
      return entry_ == other.entry_;
    }

    explicit operator bool() const {
      return entry_ != term_;
    }

  private:
    friend class LinkedList;

    Iter(LinkedListEntry* entry, LinkedListEntry* term, bool reverse = false)
      : entry_(entry), term_(term), reverse_(reverse) {}

    LinkedListEntry* entry_;
    LinkedListEntry* term_;
    bool reverse_;
  };

  Iter begin() {
    return Iter(sentinel_.next_, &sentinel_);
  }

  Iter end() {
    return Iter(&sentinel_, &sentinel_);
  }

  Iter rbegin() {
    return Iter(sentinel_.prev_, &sentinel_, true);
  }

  Iter rend() {
    return Iter(&sentinel_, &sentinel_, true);
  }

  void PushFront(LinkedListEntry& entry) {
    sentinel_.InsertAfter(entry);
  }

  void PushBack(LinkedListEntry& entry) {
    sentinel_.InsertBefore(entry);
  }

  T* PopFront() {
    LinkedListEntry* entry = sentinel_.next_;
    assert_ne(entry, &sentinel_);
    entry->Remove();
    return ElementFrom(entry);
  }

  T* PopBack() {
    LinkedListEntry* entry = sentinel_.prev_;
    assert_ne(entry, &sentinel_);
    entry->Remove();
    return ElementFrom(entry);
  }

  bool IsEmpty() const {
    return sentinel_.next_ == &sentinel_;
  }

private:
  static T* ElementFrom(LinkedListEntry* entry) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(entry) - entry_offset);
  }

  LinkedListEntry sentinel_;
};

#endif
