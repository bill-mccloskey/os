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
void sys_write_byte(char c);
void sys_reschedule();
void sys_exit_thread();
}

void PrintString(const char* s) {
  for (int i = 0; s[i]; i++) {
    sys_write_byte(s[i]);
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

  PrintString("Hello from the program!\n");
  sys_exit_thread();
}
}
