#include "thread.h"

#include "assertions.h"
#include "frame_allocator.h"
#include "page_translation.h"
#include "protection.h"
#include "serial.h"

// FIXME: This code is turning into spaghetti. Too many mutual dependencies between
// thread.cc, protection.cc, and interrupts.cc. Is there a better way to split it?
// Or should it all be in one file?

Scheduler* g_scheduler;

Thread::Thread(virt_addr_t start_func,
               virt_addr_t stack_ptr,
               const RefPtr<AddressSpace>& address_space,
               int priority)
  : state_{},
    start_func_(start_func),
    address_space_(address_space),
    priority_(priority) {
  assert_lt(priority, Scheduler::kNumQueues);

  state_.rip = start_func;
  state_.cs = SegmentSelector(kUserCodeSegmentIndex, kUserPrivilege).Serialize();
  state_.rflags = (1 << 9); // Enable interrupts.
  state_.rsp = stack_ptr;
  state_.ss = SegmentSelector(kUserStackSegmentIndex, kUserPrivilege).Serialize();
}

void Thread::SetKernelThread() {
  state_.cs = SegmentSelector(kKernelCodeSegmentIndex, kKernelPrivilege).Serialize();
  state_.ss = SegmentSelector(kKernelStackSegmentIndex, kKernelPrivilege).Serialize();
}

void Thread::AllowIo() {
  state_.rflags |= 3 << 12; // Set the IOPL to 3.
}

void Thread::Start() {
  assert_eq(status_, kBlocked);
  status_ = kRunnable;
  g_scheduler->Enqueue(this);
}

Scheduler::Scheduler(virt_addr_t syscall_stack_reservation) {
  assert_eq((syscall_stack_reservation + sizeof(CpuState)) % kPageSize, 0);
  cpu_state_ = reinterpret_cast<CpuState*>(syscall_stack_reservation);
  *cpu_state_ = {};
}

void Scheduler::Enqueue(Thread* thread) {
  int prio = thread->priority();

  assert_eq(thread->prev_thread_, nullptr);
  assert_eq(thread->next_thread_, nullptr);

  Thread* head = queue_head_[prio];
  queue_head_[prio] = thread;
  thread->next_thread_ = head;
  if (head) {
    head->prev_thread_ = thread;
  } else {
    queue_tail_[prio] = thread;
  }
}

Thread* Scheduler::Dequeue() {
  for (int i = 0; i < kNumQueues; i++) {
    if (!queue_tail_[i]) continue;

    Thread* result = queue_tail_[i];
    queue_tail_[i] = result->prev_thread_;
    if (result->prev_thread_) {
      result->prev_thread_->next_thread_ = nullptr;
    } else {
      queue_head_[i] = nullptr;
    }

    result->prev_thread_ = nullptr;
    return result;
  }

  return nullptr;
}

Allocator<Thread>* g_thread_allocator;
DEFINE_ALLOCATION_METHODS(Thread, g_thread_allocator);

extern "C" {
void switch_address_space(phys_addr_t tables);
}

void Scheduler::Reschedule(bool requeue) {
  if (running_thread_) {
    cpu_state_->previous_thread = &running_thread_->state_;

    if (requeue) {
      running_thread_->status_ = Thread::kRunnable;
      Enqueue(running_thread_);
    }

    running_thread_ = nullptr;
  } else {
    cpu_state_->previous_thread = nullptr;
  }

  Thread* thread = Dequeue();
  assert(thread);

  g_serial->Printf("Scheduling thread %p\n", thread->state_.rip);

  running_thread_ = thread;
  thread->status_ = Thread::kRunning;

  cpu_state_->current_thread = &thread->state_;

  switch_address_space(thread->address_space_->table_root());
}

void Scheduler::ExitThread() {
  Thread* thread = running_thread_;
  assert(thread);

  Reschedule(/*requeue=*/ false);

  delete thread;
}

extern "C" {
  void scheduler_start(ThreadState* state);
}

void Scheduler::Start() {
  Reschedule();
  scheduler_start(&running_thread_->state_);
}

void Scheduler::DumpState() {
  Thread* thread = running_thread_;
  g_serial->Printf("rip = %p\n", thread->state_.rip);
}
