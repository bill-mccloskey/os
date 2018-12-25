#include "assertions.h"
#include "framebuffer.h"
#include "emulator.h"
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

class ConsoleEmulatorOutput : public TerminalEmulatorOutput {
public:
  ConsoleEmulatorOutput(FrameBuffer* fb) : fb_(fb) {}

  int SelectFrameBufferColor(CellAttributes::Color color, bool is_fg, bool inverse) {
    switch (color) {
      case CellAttributes::kBlack:
        return inverse ? (is_fg ? FrameBuffer::kWhite : FrameBuffer::kLightGray) : FrameBuffer::kBlack;
        break;

      case CellAttributes::kRed:
        return FrameBuffer::kRed;
        break;

      case CellAttributes::kGreen:
        return FrameBuffer::kGreen;
        break;

      case CellAttributes::kYellow:
        return is_fg ? FrameBuffer::kYellow : FrameBuffer::kBrown;
        break;

      case CellAttributes::kBlue:
        return FrameBuffer::kBlue;
        break;

      case CellAttributes::kMagenta:
        return FrameBuffer::kMagenta;
        break;

      case CellAttributes::kCyan:
        return FrameBuffer::kCyan;
        break;

      case CellAttributes::kWhite:
        return inverse ? FrameBuffer::kBlack : (is_fg ? FrameBuffer::kWhite : FrameBuffer::kLightGray);
        break;

      case CellAttributes::kDefault:
        return SelectFrameBufferColor(is_fg ? CellAttributes::kWhite : CellAttributes::kBlack, is_fg, inverse);
        break;
    }
  }

  void SetCell(int x, int y, wchar_t ch, const CellAttributes& attrs) override {
    assert_ge(x, 0);
    assert_ge(y, 0);

    if (x < FrameBuffer::kWidth && y < FrameBuffer::kHeight) {
      fb_->WriteCell(x, y, ch,
                     SelectFrameBufferColor(attrs.fg_color, true, attrs.HasAttribute(CellAttributes::kInverse)),
                     SelectFrameBufferColor(attrs.bg_color, false, attrs.HasAttribute(CellAttributes::kInverse)));
    }
  }

  void Bell() override {}

  void MoveCells(int dest_x, int dest_y, int src_x, int src_y, int width, int height) override {
    int start_x, end_x, x_incr;
    if (dest_x <= src_x) {
      start_x = 0;
      end_x = width;
      x_incr = 1;
    } else {
      start_x = width - 1;
      end_x = -1;
      x_incr = -1;
    }

    int start_y, end_y, y_incr;
    if (dest_y <= src_y) {
      start_y = 0;
      end_y = height;
      y_incr = 1;
    } else {
      start_y = height - 1;
      end_y = -1;
      y_incr = -1;
    }

    for (int y = start_y; y != end_y; y += y_incr) {
      for (int x = start_x; x != end_x; x += x_incr) {
        fb_->CopyCell(dest_x + x, dest_y + y, src_x + x, src_y + y);
      }
    }
  }

  void CopyToScrollback(int src_y, int y_count) override {}

  int Width() override { return FrameBuffer::kWidth; }
  int Height() override { return FrameBuffer::kHeight; }

private:
  FrameBuffer* fb_;
};

extern "C" {
void _start() {
  IoPorts io;
  FrameBuffer fb((char*)0xb8000, &io);

  fb.Clear();

  DebugOutputStream stream;

  stream.Printf("terminal: Hello from the terminal!\n");

  ConsoleEmulatorOutput out(&fb);
  TerminalEmulator em(&out);

  const char* input = "Welcome to \e[32mCalic\e[33mOS\e[m!";
  for (int i = 0; input[i]; i++) {
    em.Input(input[i]);
  }

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
