#ifndef thread_h
#define thread_h

#include "types.h"

class Scheduler;

struct ThreadState {
  // Automatically pushed to the stack during interrupt handling.
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;

  // Callee-saved registers.
  uint64_t rax;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
};

struct CpuState {
  ThreadState* current_thread;
  ThreadState* previous_thread;
};

class Thread {
public:
  Thread(virt_addr_t start_func,
         virt_addr_t stack_ptr,
         phys_addr_t page_tables,
         int priority,
         bool kernel_thread = false);

  void Start();

  int priority() const { return priority_; }

private:
  friend class Scheduler;

  enum Status {
    kRunnable,
    kRunning,
    kBlocked,
  };

  ThreadState state_;
  virt_addr_t start_func_;
  phys_addr_t page_tables_;
  int priority_;
  bool kernel_thread_;
  Status status_ = kBlocked;
  Thread* next_thread_ = nullptr;
  Thread* prev_thread_ = nullptr;
};

class Scheduler {
public:
  Scheduler();

  // Starts the scheduler and runs the highest priority thread.
  void Start();

  // Schedules a different thread to run upon returning to user space.
  void Reschedule(bool requeue = true);

  void ExitThread();

  // For debugging. Dumps to serial port.
  void DumpState();

  ThreadState* current_thread_state() { return &running_thread_->state_; }

private:
  friend class Thread;

  void Enqueue(Thread* thread);
  Thread* Dequeue();

  static const int kNumQueues = 3;

  CpuState* cpu_state_;

  Thread* running_thread_ = nullptr;
  Thread* queue_head_[kNumQueues] = {};
  Thread* queue_tail_[kNumQueues] = {};
};

extern Scheduler* g_scheduler;

#endif // thread_h
