#ifndef system_h
#define system_h

#include "types.h"

extern "C" {
void SysWriteByte(char c);
void SysReschedule();
void SysExitThread();

void SysSend(int dest_tid, int type, uint64_t payload);
void SysReceive(int* sender_tid, int* type, uint64_t* payload);
void SysNotify(int notify_tid);

void SysRequestInterrupt(int irq);
void SysAckInterrupt(int irq);
}

#endif
