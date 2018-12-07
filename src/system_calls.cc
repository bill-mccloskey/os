#include "assertions.h"
#include "serial.h"
#include "thread.h"
#include "types.h"

typedef void (*GenericSysCall)();

void SysNull() {
  panic("SysNull called");
}

void SysWriteByte(char c) {
  g_serial->WriteByte(c);
}

void SysReschedule() {
  g_scheduler->Reschedule();
}

void SysExitThread() {
  g_scheduler->ExitThread();
}

#define REGISTER_SYSCALL(fn) reinterpret_cast<GenericSysCall>(fn)
extern "C" {
GenericSysCall syscall_handler_table[256] = {
  REGISTER_SYSCALL(SysNull),
  REGISTER_SYSCALL(SysWriteByte),
  REGISTER_SYSCALL(SysReschedule),
  REGISTER_SYSCALL(SysExitThread),
};
}

