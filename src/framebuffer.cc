#include "framebuffer.h"

#include "io.h"

/* The I/O ports */
static const int kCommandPort = 0x3D4;
static const int kDataPort = 0x3D5;

/* The I/O port commands */
static const int kHighByteCommand = 14;
static const int kLowByteCommand = 15;

FrameBuffer::FrameBuffer(char* fb, IoPorts* io)
  : fb_(fb), io_(io) {}

void FrameBuffer::MoveCursor(int x, int y)
{
  int pos = y * kWidth + x;

  io_->Out(kCommandPort, kHighByteCommand);
  io_->Out(kDataPort, ((pos >> 8) & 0x00FF));
  io_->Out(kCommandPort, kLowByteCommand);
  io_->Out(kDataPort, pos & 0x00FF);
}

void FrameBuffer::WriteCell(int x, int y, char c, int fg, int bg) {
  int pos = y * kWidth * 2 + x * 2;

  fb_[pos] = c;
  fb_[pos + 1] = ((fg & 0x0f) << 4) | (bg & 0x0f);
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
      WriteCell(x, y, ' ', kBlack, kWhite);
    }
  }
}
