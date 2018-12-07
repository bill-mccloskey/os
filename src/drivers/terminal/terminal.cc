#include "framebuffer.h"
#include "io.h"

struct S {
  int a;
  char b;
};

S global1[1000];
S global2[1000] = {{100, 10}};

void func() {
  global1[0].a = global2[0].a;
}

using uint64_t = unsigned long;

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

static void num_to_string(long n, char* buf, int base) {
  long digits = 0;
  long x = n;
  while (x != 0) {
    x /= base;
    digits++;
  }

  if (n == 0) {
    buf[0] = '0';
    buf[1] = 0x00;
    return;
  }

  if (n < 0) {
    buf[0] = '-';
    buf++;
    n = -n;
  }

  buf[digits] = 0x00;
  while (n != 0) {
    int digit = n % base;
    if (digit <= 9) {
      buf[--digits] = digit + '0';
    } else {
      buf[--digits] = digit - 10 + 'a';
    }
    n /= base;
  }
}

extern "C" {
void _start() {
  IoPorts io;
  FrameBuffer fb((char*)0xb8000, &io);

  fb.Clear();

  fb.WriteString("Hello from user space!\n", FrameBuffer::kWhite, FrameBuffer::kBlack);
  fb.WriteString("This is a test\n", FrameBuffer::kCyan, FrameBuffer::kRed);
  for (int i = 0; i < 10; i++) {
    fb.WriteString("This is a line\n", FrameBuffer::kCyan, FrameBuffer::kBlack);
  }
  fb.MoveCursor(0, 0);

  fb.ScrollTo(5);

  PrintString("terminal: Hello from the terminal!\n");

  SysRequestInterrupt(1);

  for (;;) {
    int sender, type, payload;
    SysReceive(&sender, &type, &payload);
    PrintString("terminal: got key ");

    unsigned char scan_code = inb(0x60);
    char buf[32];
    num_to_string(scan_code, buf, 10);
    PrintString(buf);
    PrintString("\n");

    SysAckInterrupt(1);
  }

  SysExitThread();
}
}
