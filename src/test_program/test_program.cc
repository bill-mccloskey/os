extern "C" {
void SysWriteByte(char c);
void SysReschedule();
void SysExitThread();

void SysSend(int dest_tid, int type, int payload);
void SysReceive(int* sender_tid, int* type, int* payload);
void SysNotify(int notify_tid);

void SysRequestInterrupt(int irq);
void SysAckInterrupt(int irq);
}

void PrintString(const char* s) {
  for (int i = 0; s[i]; i++) {
    SysWriteByte(s[i]);
  }
}

extern "C" {
void _start() {
  PrintString("test_program: Hello from the test program!\n");

  //sys_send(1, 18, 9857);
  //SysNotify(1);

  PrintString("test_program: Finished send\n");

  SysExitThread();
}
}
