#include "assertions.h"
#include "interrupts.h"
#include "serial.h"
#include "thread.h"
#include "types.h"

typedef void (*GenericSysCall)();

void SysNull() {
  panic("SysNull called");
}

void SysWriteByte(char c) {
  g_serial->OutputChar(c);
}

void SysReschedule() {
  g_scheduler->Reschedule();
}

void SysExitThread() {
  g_scheduler->ExitThread();
}

void SysSend(int dest_tid, int type, int payload) {
  g_scheduler->current_thread()->Send(dest_tid, type, payload);
}

void SysReceive(int* sender_tid, int* type, int* payload) {
  g_scheduler->current_thread()->Receive(sender_tid, type, payload);
}

void SysNotify(int notify_tid) {
  g_scheduler->current_thread()->Notify(notify_tid);
}

void SysRequestInterrupt(int irq) {
  // FIXME: Lock this down so only some processes can use it.
  g_interrupts->RegisterForInterrupt(irq, g_scheduler->current_thread());
}

void SysAckInterrupt(int irq) {
  // FIXME: Lock this down so only some processes can use it.
  // Also ensure it's only used by processes registered for the interrupt.
  // And maybe check that the interrupt is outstanding?
  g_interrupts->Acknowledge(irq);
}

#define REGISTER_SYSCALL(fn) reinterpret_cast<GenericSysCall>(fn)
extern "C" {
GenericSysCall syscall_handler_table[256] = {
  REGISTER_SYSCALL(SysNull),
  REGISTER_SYSCALL(SysWriteByte),
  REGISTER_SYSCALL(SysReschedule),
  REGISTER_SYSCALL(SysExitThread),
  REGISTER_SYSCALL(SysSend),
  REGISTER_SYSCALL(SysReceive),
  REGISTER_SYSCALL(SysNotify),
  REGISTER_SYSCALL(SysRequestInterrupt),
  REGISTER_SYSCALL(SysAckInterrupt),
};
}

