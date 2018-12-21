#include "thread.h"

#include "assertions.h"
#include "frame_allocator.h"
#include "interrupts.h"
#include "page_translation.h"
#include "protection.h"
#include "serial.h"

Scheduler* g_scheduler;
int g_thread_id = 32;

Thread::Thread(virt_addr_t start_func,
               virt_addr_t stack_ptr,
               const RefPtr<AddressSpace>& address_space,
               int priority)
  : id_(g_thread_id++),
    state_{},
    address_space_(address_space),
    priority_(priority) {
  assert_lt(priority, Scheduler::kNumQueues);

  state_.rip = start_func;
  state_.cs = SegmentSelector(kUserCodeSegmentIndex, kUserPrivilege).Serialize();
  state_.rflags = (1 << 9); // Enable interrupts.
  state_.rsp = stack_ptr;
  state_.ss = SegmentSelector(kUserStackSegmentIndex, kUserPrivilege).Serialize();
}

Thread::~Thread() {
  assert(!thread_links.InList());
  g_scheduler->RemoveThread(this);

  // FIXME: Would be good to have a general notification mechanism for when a thread exits.
  g_interrupts->UnregisterForInterrupts(this);
}

void Thread::SetKernelThread() {
  state_.cs = SegmentSelector(kKernelCodeSegmentIndex, kKernelPrivilege).Serialize();
  state_.ss = SegmentSelector(kKernelStackSegmentIndex, kKernelPrivilege).Serialize();
}

void Thread::AllowIo() {
  state_.rflags |= 3 << 12; // Set the IOPL to 3.
}

void Thread::Start() {
  assert_eq(status_, kStarting);
  status_ = kRunnable;
  g_scheduler->AddThread(this);
  g_scheduler->Enqueue(this);
}

void Thread::Send(int dest_tid, int type, uint64_t payload) {
  Thread* dest = g_scheduler->FindThread(dest_tid);
  // FIXME: Check for null dest.
  if (dest->status_ == kBlockedReceiving) {
    g_scheduler->RunThread(dest, true);

    // This must happen *after* RunThread so we're in the AS of the recipient.
    // Warning: This is writing directly into the userspace. Could get page faults
    // and maybe other exceptions? Need to handle this!
    ReceiveInfo& info = dest->receive_info_;
    *info.sender_tid = id();
    *info.type = type;
    *info.payload = payload;
  } else {
    send_info_.sender_tid = id();
    send_info_.type = type;
    send_info_.payload = payload;

    dest->send_queue_.PushBack(thread_links);
    status_ = kBlockedSending;
    g_scheduler->Reschedule(false);
  }
}

void Thread::Receive(int* sender_tid, int* type, uint64_t* payload) {
  if (notified_) {
    notified_ = false;
    *sender_tid = 0;
    *type = 0;
    *payload = 0;
    return;
  }

  if (send_queue_.IsEmpty()) {
    receive_info_.sender_tid = sender_tid;
    receive_info_.type = type;
    receive_info_.payload = payload;

    status_ = kBlockedReceiving;
    g_scheduler->Reschedule(false);
  } else {
    Thread* sender = send_queue_.PopFront();
    assert_eq(sender->status_, kBlockedSending);
    sender->status_ = kRunnable;

    const SendInfo& info = sender->send_info_;
    *sender_tid = info.sender_tid;
    *type = info.type;
    *payload = info.payload;

    g_scheduler->Enqueue(sender);
  }
}

void Thread::Notify(int notify_tid) {
  Thread* dest = g_scheduler->FindThread(notify_tid);
  // FIXME: Check for null dest.
  if (dest->status_ == kBlockedReceiving) {
    g_scheduler->RunThread(dest, true);

    // This must happen *after* RunThread so we're in the AS of the recipient.
    // Warning: This is writing directly into the userspace. Could get page faults
    // and maybe other exceptions? Need to handle this!
    ReceiveInfo& info = dest->receive_info_;
    *info.sender_tid = 0;
    *info.type = 0;
    *info.payload = 0;
  } else {
    dest->notified_ = true;
  }
}

void Thread::NotifyFromKernel() {
  if (status_ == kBlockedReceiving) {
    g_scheduler->RunThread(this, true);

    // This must happen *after* RunThread so we're in the AS of the recipient.
    // Warning: This is writing directly into the userspace. Could get page faults
    // and maybe other exceptions? Need to handle this!
    ReceiveInfo& info = receive_info_;
    *info.sender_tid = 0;
    *info.type = 0;
    *info.payload = 0;
  } else {
    notified_ = true;
  }
}

Scheduler::Scheduler(virt_addr_t syscall_stack_reservation)
  : thread_id_hash_{} {
  assert_eq((syscall_stack_reservation + sizeof(CpuState)) % kPageSize, 0);
  cpu_state_ = reinterpret_cast<CpuState*>(syscall_stack_reservation);
  *cpu_state_ = {};
}

void Scheduler::Enqueue(Thread* thread) {
  int prio = thread->priority();
  runnable_[prio].PushBack(thread->thread_links);
}

Thread* Scheduler::Dequeue() {
  for (int i = 0; i < kNumQueues; i++) {
    if (runnable_[i].IsEmpty()) continue;
    return runnable_[i].PopFront();
  }

  return nullptr;
}

Allocator<Thread>* g_thread_allocator;
DEFINE_ALLOCATION_METHODS(Thread, g_thread_allocator);

extern "C" {
void SwitchAddressSpace(phys_addr_t tables);
void SchedulerStart(ThreadState* state);
}

void Scheduler::RunThread(Thread* thread, bool requeue) {
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

  //g_serial->Printf("Scheduling thread %p\n", (void*)thread->state_.rip);

  running_thread_ = thread;
  thread->status_ = Thread::kRunning;

  cpu_state_->current_thread = &thread->state_;
  SwitchAddressSpace(thread->address_space_->table_root());
}

void Scheduler::Reschedule(bool requeue) {
  Thread* thread = Dequeue();
  assert(thread);

  RunThread(thread, requeue);
}

void Scheduler::ExitThread() {
  Thread* thread = running_thread_;
  assert(thread);

  Reschedule(/*requeue=*/ false);

  delete thread;
}

void Scheduler::Start() {
  Reschedule();
  SchedulerStart(&running_thread_->state_);
}

void Scheduler::DumpState() {
  Thread* thread = running_thread_;
  g_serial->Printf("rip = %p\n", (void*)thread->state_.rip);
}

void Scheduler::AddThread(Thread* thread) {
  assert_ge(thread->id(), 0);
  int h = thread->id() % kThreadIdHashSize;
  thread->next_by_id_ = thread_id_hash_[h];
  thread_id_hash_[h] = thread;
}

void Scheduler::RemoveThread(Thread* thread) {
  assert_ge(thread->id(), 0);
  int h = thread->id() % kThreadIdHashSize;
  for (Thread** t = &thread_id_hash_[h]; *t; t = &(*t)->next_by_id_) {
    if (*t == thread) {
      *t = thread->next_by_id_;
      return;
    }
  }

  panic("Thread not found in hash table");
}

Thread* Scheduler::FindThread(int id) {
  assert_ge(id, 0);
  int h = id % kThreadIdHashSize;
  for (Thread* t = thread_id_hash_[h]; t; t = t->next_by_id_) {
    if (t->id() == id) {
      return t;
    }
  }

  panic("Thread ID not found in hash table");
  return nullptr;
}
