#include "raster_frame_buffer.h"

#include "base/assertions.h"
#include "drivers/console/console_fonts.h"

#include <string.h>

RasterFrameBuffer::RasterFrameBuffer(FrameBufferData* fb, FontName font) : fb_(fb) {
  // See http://www.fifi.org/doc/console-tools-dev/file-formats/psf.
  static const int kPSFMagic0 = 0x36;
  static const int kPSFMagic1 = 0x04;
  static const int kFileMode = 2;

  if (font == VGA) {
    font_ = GetFont_VGA();
  } else {
    font_ = GetFont_Fixed();
  }

  assert_eq(font_[0], kPSFMagic0);
  assert_eq(font_[1], kPSFMagic1);
  assert_eq(font_[2], kFileMode);

  glyph_height_ = font_[3];
}

unsigned char* RasterFrameBuffer::GlyphData(int ch) {
  static const int kHeaderSize = 4;
  return font_ + kHeaderSize + (ch * glyph_height_);
}

int RasterFrameBuffer::Width() {
  return fb_->width / kGlyphWidth;
}

int RasterFrameBuffer::Height() {
  return fb_->height / glyph_height_;
}

void RasterFrameBuffer::DrawCursor(int x, int y) {
  if (x < 0 || y < 0) return;

  x *= kGlyphWidth;
  y *= glyph_height_;

  char* dst = (char*)fb_->addr + y * fb_->pitch + x * (fb_->bpp / 8);

  for (int row = 0; row < glyph_height_; row++) {
    for (int col = 7; col >= 0; col--) {
      *dst = *dst ^ 0xff;
      dst++;
      *dst = *dst ^ 0xff;
      dst++;
      *dst = *dst ^ 0xff;
      dst++;
      dst++;
    }
    dst += fb_->pitch - 8 * (fb_->bpp / 8);
  }
}

void RasterFrameBuffer::MoveCursor(int x, int y) {
  DrawCursor(x_, y_);
  x_ = x;
  y_ = y;
  DrawCursor(x_, y_);
}

void RasterFrameBuffer::SelectColor(Color color, bool is_fg, int* red, int* green, int* blue) {
  switch (color) {
    case kBlack:
      *red = 0x00;
      *green = 0x00;
      *blue = 0x00;
      break;

    case kBlue:
      *red = 0x00;
      *green = 0x00;
      *blue = 0xff;
      break;

    case kGreen:
      *red = 0x00;
      *green = 0xff;
      *blue = 0x00;
      break;

    case kCyan:
      *red = 0x00;
      *green = 0xff;
      *blue = 0xff;
      break;

    case kRed:
      *red = 0xff;
      *green = 0x00;
      *blue = 0x00;
      break;

    case kMagenta:
      *red = 0xff;
      *green = 0x00;
      *blue = 0xff;
      break;

    case kBrown:
      *red = 0x96;
      *green = 0x4b;
      *blue = 0x00;
      break;

    case kLightGray:
      *red = 0xd3;
      *green = 0xd3;
      *blue = 0xd3;
      break;

    case kYellow:
      *red = 0xff;
      *green = 0xff;
      *blue = 0x00;
      break;

    case kWhite:
      *red = 0xff;
      *green = 0xff;
      *blue = 0xff;
      break;

    case kDefault:
      if (is_fg) {
        *red = 0xff;
        *green = 0xff;
        *blue = 0xff;
      } else {
        *red = 0x00;
        *green = 0x00;
        *blue = 0x00;
      }
      break;
  }
}

void RasterFrameBuffer::WriteCell(int x, int y, char c, Color fg, Color bg) {
  int cursor_mask = 0;
  if (x == x_ && y == y_) cursor_mask = 0xff;

  x *= kGlyphWidth;
  y *= glyph_height_;

  char* dst = (char*)fb_->addr + y * fb_->pitch + x * (fb_->bpp / 8);
  unsigned char* src = GlyphData(c);

  int fg_red, fg_green, fg_blue;
  int bg_red, bg_green, bg_blue;

  SelectColor(fg, true, &fg_red, &fg_green, &fg_blue);
  SelectColor(bg, false, &bg_red, &bg_green, &bg_blue);

  for (int row = 0; row < glyph_height_; row++) {
    for (int col = 7; col >= 0; col--) {
      if (*src & (1 << col)) {
        *dst++ = fg_blue ^ cursor_mask;
        *dst++ = fg_green ^ cursor_mask;
        *dst++ = fg_red ^ cursor_mask;
        dst++;
      } else {
        *dst++ = bg_blue ^ cursor_mask;
        *dst++ = bg_green ^ cursor_mask;
        *dst++ = bg_red ^ cursor_mask;
        dst++;
      }
    }
    src++;
    dst += fb_->pitch - 8 * (fb_->bpp / 8);
  }
}

void RasterFrameBuffer::Clear() {
  memset((char*)fb_->addr, 0xd3, fb_->pitch * fb_->height);
  DrawCursor(x_, y_);
}

void RasterFrameBuffer::CopyCell(int dst_x, int dst_y, int src_x, int src_y) {
}

void RasterFrameBuffer::ScrollTo(int line) {
}
