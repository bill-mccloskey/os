#ifndef thread_h
#define thread_h

#include "address_space.h"
#include "allocator.h"
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
         const RefPtr<AddressSpace>& address_space,
         int priority);

  // The thread runs with CPL = 0.
  void SetKernelThread();

  // Thread can call in/out instructions.
  void AllowIo();

  void Start();

  int priority() const { return priority_; }

  DECLARE_ALLOCATION_METHODS();

private:
  friend class Scheduler;

  enum Status {
    kRunnable,
    kRunning,
    kBlocked,
  };

  ThreadState state_;
  virt_addr_t start_func_;
  RefPtr<AddressSpace> address_space_;
  int priority_;
  Status status_ = kBlocked;
  Thread* next_thread_ = nullptr;
  Thread* prev_thread_ = nullptr;
};

extern Allocator<Thread>* g_thread_allocator;

class Scheduler {
public:
  Scheduler(virt_addr_t syscall_stack_reservation);

  // Starts the scheduler and runs the highest priority thread.
  void Start();

  // Schedules a different thread to run upon returning to user space.
  void Reschedule(bool requeue = true);

  void ExitThread();

  // For debugging. Dumps to serial port.
  void DumpState();

  ThreadState* current_thread_state() { return &running_thread_->state_; }

  // Amount of space to reserve at the top of the syscall stack for the scheduler.
  static size_t SysCallStackAdjustment() { return sizeof(CpuState); }

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
