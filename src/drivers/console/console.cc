#include "framebuffer.h"
#include "io.h"
#include "output_stream.h"

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

class DebugOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
    SysWriteByte(c);
  }
};

// Line discipline: You put characters from the keyboard (or whatever) into it.
// It keeps a buffer for the program to read from. It also decides when that buffer
// should be "released" to the program. Finally, it decides what characters should
// be echoed to the screen.

// The terminal display takes characters in, either from the program or from the
// line displine.

// Should read commands that are sent to the terminal driver be synchronous or
// asynchronous? Seems like async would be better. You'll ask it to start a read,
// and it will tell you when the read is done.

// How will the Unix process manage all the I/O it has to do with various drivers
// and things? Perhaps it would be better if it used lots of threads, and each
// user thread communicated with its own thread in the Unix process? Then it would
// make more sense for driver I/O to be sync.

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

  DebugOutputStream stream;

  stream.Printf("terminal: Hello from the terminal!\n");

  for (;;) {
    int sender, type, payload;
    SysReceive(&sender, &type, &payload);
  }

  SysExitThread();
}

void __cxa_pure_virtual() {
  for (;;) {}
}
}
