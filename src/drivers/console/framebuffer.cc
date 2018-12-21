#include "framebuffer.h"

#include "io.h"

// The I/O ports. To change a VGA register, first set the register location by writing it to kAddressPort.
// Then read or write the value by reading or writing from kDataPort.
static const int kAddressPort = 0x3d4;
static const int kDataPort = 0x3d5;

// Register locations:
static const int kMaxScanLineReg = 9;
static const int kCursorStartReg = 10;
static const int kCursorEndReg = 11;
static const int kCursorLocationHighReg = 14;
static const int kCursorLocationLowReg = 15;

/* See http://www.osdever.net/FreeVGA/home.htm for documentation. */

FrameBuffer::FrameBuffer(char* fb, IoPorts* io)
  : fb_(fb), io_(io) {
  // See http://www.osdever.net/FreeVGA/vga/crtcreg.htm for docs on these registers.

  // Find the maximum scan line (i.e., the height of a character - 1).
  io_->Out(kAddressPort, kMaxScanLineReg);
  int max_scan_line = io_->In(kDataPort) & 0x1f;

  // Enable the cursor and set the "cursor scan line start" to 0.
  io_->Out(kAddressPort, kCursorStartReg);
  io_->Out(kDataPort, 0);

  io_->Out(kAddressPort, kCursorEndReg);
  io_->Out(kDataPort, max_scan_line);
}

void FrameBuffer::MoveCursor(int x, int y) {
  int pos = y * kWidth + x;

  io_->Out(kAddressPort, kCursorLocationHighReg);
  io_->Out(kDataPort, ((pos >> 8) & 0xff));
  io_->Out(kAddressPort, kCursorLocationLowReg);
  io_->Out(kDataPort, pos & 0xff);
}

void FrameBuffer::WriteCell(int x, int y, char c, int fg, int bg) {
  //assert_le(bg, kMaxBackgroundColor);

  int pos = y * kWidth * 2 + x * 2;

  fb_[pos] = c;
  fb_[pos + 1] = ((bg & 0x0f) << 4) | (fg & 0x0f);
}

void FrameBuffer::WriteChar(char c, int fg, int bg) {
  if (c != '\n') {
    WriteCell(x_, y_, c, fg, bg);
  }

  if (c == '\n') {
    x_ = 0;
    y_++;
  } else {
    x_++;
  }
}

void FrameBuffer::WriteString(const char* str, int fg, int bg) {
  for (int i = 0; str[i]; i++) {
    WriteChar(str[i], fg, bg);
  }

  MoveCursor(x_, y_);
}

void FrameBuffer::Clear() {
  for (int x = 0; x < kWidth; x++) {
    for (int y = 0; y < kHeight; y++) {
      WriteCell(x, y, ' ', kBlack, kBlack);
    }
  }
}

void FrameBuffer::ScrollTo(int line) {
  int start_address = kWidth * line;

  io_->Out(kAddressPort, 0xc);
  io_->Out(kDataPort, start_address >> 8);

  io_->Out(kAddressPort, 0xd);
  io_->Out(kDataPort, start_address & 0xff);
}
