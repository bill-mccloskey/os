#ifndef thread_h
#define thread_h

#include "address_space.h"
#include "allocator.h"
#include "linked_list.h"
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

struct SendInfo {
  int sender_tid;
  int type;
  int payload;
};

struct ReceiveInfo {
  int* sender_tid;
  int* type;
  int* payload;
};

class Thread {
public:
  Thread(virt_addr_t start_func,
         virt_addr_t stack_ptr,
         const RefPtr<AddressSpace>& address_space,
         int priority);
  ~Thread();

  // The thread runs with CPL = 0.
  void SetKernelThread();

  // Thread can call in/out instructions.
  void AllowIo();

  void Start();

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }
  int priority() const { return priority_; }

  void Send(int dest_tid, int type, int payload);
  void Receive(int* sender_tid, int* type, int* payload);
  void Notify(int notify_tid);
  void NotifyFromKernel();

  DECLARE_ALLOCATION_METHODS();

private:
  friend class Scheduler;

  enum Status {
    kStarting,
    kRunnable,
    kRunning,
    kBlockedReceiving,
    kBlockedSending
  };

  // Must be the first member!
  LinkedListEntry thread_links;

  int id_;
  ThreadState state_;
  RefPtr<AddressSpace> address_space_;
  int priority_;
  Status status_ = kStarting;
  LinkedList<Thread, 0> send_queue_;

  // The next link for the thread ID hashtable.
  Thread* next_by_id_ = nullptr;

  // For IPC.
  SendInfo send_info_;
  ReceiveInfo receive_info_;
  bool notified_ = false;
};

extern Allocator<Thread>* g_thread_allocator;

class Scheduler {
public:
  Scheduler(virt_addr_t syscall_stack_reservation);

  // Starts the scheduler and runs the highest priority thread.
  void Start();

  // Schedules a different thread to run upon returning to user space.
  void RunThread(Thread* thread, bool requeue = true);
  void Reschedule(bool requeue = true);

  Thread* FindThread(int id);

  void ExitThread();

  // For debugging. Dumps to serial port.
  void DumpState();

  Thread* current_thread() const { return running_thread_; }

  // Amount of space to reserve at the top of the syscall stack for the scheduler.
  static size_t SysCallStackAdjustment() { return sizeof(CpuState); }

private:
  friend class Thread;

  void AddThread(Thread* thread);
  void RemoveThread(Thread* thread);

  void Enqueue(Thread* thread);
  Thread* Dequeue();

  static const int kNumQueues = 3;
  static const int kThreadIdHashSize = 16384;

  CpuState* cpu_state_;

  Thread* running_thread_ = nullptr;
  LINKED_LIST(Thread, thread_links) runnable_[kNumQueues];

  Thread* thread_id_hash_[kThreadIdHashSize];
};

extern Scheduler* g_scheduler;

#endif // thread_h
