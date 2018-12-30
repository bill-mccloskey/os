#include "text_frame_buffer.h"
#include "assertions.h"

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

TextFrameBuffer::TextFrameBuffer(char* fb, IoPorts* io)
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

void TextFrameBuffer::MoveCursor(int x, int y) {
  int pos = y * kWidth + x;

  io_->Out(kAddressPort, kCursorLocationHighReg);
  io_->Out(kDataPort, ((pos >> 8) & 0xff));
  io_->Out(kAddressPort, kCursorLocationLowReg);
  io_->Out(kDataPort, pos & 0xff);
}

int TextFrameBuffer::SelectColor(Color color, bool is_fg) {
  static const int kBlack = 0;
  static const int kBlue = 1;
  static const int kGreen = 2;
  static const int kCyan = 3;
  static const int kRed = 4;
  static const int kMagenta = 5;
  static const int kBrown = 6;
  static const int kLightGray = 7;

  // Only for use as foreground colors.
#if 0
  static const int kDarkGrey = 8;
  static const int kLightBlue = 9;
  static const int kLightGreen = 10;
  static const int kLightCyan = 11;
  static const int kLightRed = 12;
  static const int kLightMagenta = 13;
#endif
  static const int kYellow = 14;
  static const int kWhite = 15;

  switch (color) {
    case AbstractFrameBuffer::kBlack:
      return kBlack;
      break;
    case AbstractFrameBuffer::kBlue:
      return kBlue;
      break;
    case AbstractFrameBuffer::kGreen:
      return kGreen;
      break;
    case AbstractFrameBuffer::kCyan:
      return kCyan;
      break;
    case AbstractFrameBuffer::kRed:
      return kRed;
      break;
    case AbstractFrameBuffer::kMagenta:
      return kMagenta;
      break;
    case AbstractFrameBuffer::kBrown:
      return kBrown;
      break;
    case AbstractFrameBuffer::kLightGray:
      return kLightGray;
      break;
    case AbstractFrameBuffer::kYellow:
      if (is_fg) {
        return kYellow;
      } else {
        return kBrown;
      }
      break;
    case AbstractFrameBuffer::kWhite:
      if (is_fg) {
        return kWhite;
      } else {
        return kLightGray;
      }
      break;
    case AbstractFrameBuffer::kDefault:
      if (is_fg) {
        return kWhite;
      } else {
        return kBlack;
      }
      break;
  }
}

void TextFrameBuffer::WriteCell(int x, int y, char c, Color fg, Color bg) {
  int pos = y * kWidth * 2 + x * 2;
  int fg_num = SelectColor(fg, true);
  int bg_num = SelectColor(bg, false);

  fb_[pos] = c;
  fb_[pos + 1] = ((bg_num & 0x0f) << 4) | (fg_num & 0x0f);
}

void TextFrameBuffer::CopyCell(int dst_x, int dst_y, int src_x, int src_y) {
  int src_pos = src_y * kWidth * 2 + src_x * 2;
  int dst_pos = dst_y * kWidth * 2 + dst_x * 2;

  fb_[dst_pos] = fb_[src_pos];
  fb_[dst_pos + 1] = fb_[src_pos + 1];
}

void TextFrameBuffer::ScrollTo(int line) {
  int start_address = kWidth * line;

  io_->Out(kAddressPort, 0xc);
  io_->Out(kDataPort, start_address >> 8);

  io_->Out(kAddressPort, 0xd);
  io_->Out(kDataPort, start_address & 0xff);
}
