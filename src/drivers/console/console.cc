#include "assertions.h"
#include "raster_frame_buffer.h"
#include "text_frame_buffer.h"
#include "emulator.h"
#include "io.h"
#include "kernel_module.h"
#include "output_stream.h"
#include "system.h"

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
  ConsoleEmulatorOutput(AbstractFrameBuffer* fb) : fb_(fb) {}

  AbstractFrameBuffer::Color SelectFrameBufferColor(CellAttributes::Color color, bool is_fg, bool inverse) {
    switch (color) {
      case CellAttributes::kBlack:
        return inverse ? AbstractFrameBuffer::kWhite : AbstractFrameBuffer::kBlack;
        break;

      case CellAttributes::kRed:
        return AbstractFrameBuffer::kRed;
        break;

      case CellAttributes::kGreen:
        return AbstractFrameBuffer::kGreen;
        break;

      case CellAttributes::kYellow:
        return TextFrameBuffer::kYellow;
        break;

      case CellAttributes::kBlue:
        return TextFrameBuffer::kBlue;
        break;

      case CellAttributes::kMagenta:
        return TextFrameBuffer::kMagenta;
        break;

      case CellAttributes::kCyan:
        return TextFrameBuffer::kCyan;
        break;

      case CellAttributes::kWhite:
        return inverse ? TextFrameBuffer::kBlack : TextFrameBuffer::kWhite;
        break;

      case CellAttributes::kDefault:
        return SelectFrameBufferColor(is_fg ? CellAttributes::kWhite : CellAttributes::kBlack, is_fg, inverse);
        break;
    }
  }

  void SetCell(int x, int y, wchar_t ch, const CellAttributes& attrs) override {
    assert_ge(x, 0);
    assert_ge(y, 0);

    if (x < fb_->Width() && y < fb_->Height()) {
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

  int Width() override { return fb_->Width(); }
  int Height() override { return fb_->Height(); }

private:
  AbstractFrameBuffer* fb_;
};

extern "C" {
void _start(KernelModuleData* module_data) {
  DebugOutputStream stream;

  stream.Printf("terminal: Hello from the terminal!\n");
  stream.Printf("terminal: framebuffer is %u x %u\n", module_data->framebuffer.width, module_data->framebuffer.height);

  //IoPorts io;
  //TextFrameBuffer fb((char*)0xb8000, &io);
  RasterFrameBuffer fb(&module_data->framebuffer, RasterFrameBuffer::VGA);

  //fb.Clear();

  ConsoleEmulatorOutput out(&fb);
  TerminalEmulator em(&out);

  const char* input = "Welcome to \e[32mCalic\e[33mOS\e[m! Please start typing.\r\n";
  for (int i = 0; input[i]; i++) {
    em.Input(input[i]);
  }

  for (;;) {
    int sender, type;
    uint64_t payload;
    SysReceive(&sender, &type, &payload);

    em.Input(payload);
  }

  SysExitThread();
}

void __cxa_pure_virtual() {
  for (;;) {}
}
}
