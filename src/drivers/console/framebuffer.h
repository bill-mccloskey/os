#ifndef framebuffer_h
#define framebuffer_h

#include "io.h"

class FrameBuffer {
public:
  static const int kWidth = 80;
  static const int kHeight = 25;

  static const int kBlack = 0;
  static const int kBlue = 1;
  static const int kGreen = 2;
  static const int kCyan = 3;
  static const int kRed = 4;
  static const int kMagenta = 5;
  static const int kBrown = 6;
  static const int kLightGray = 7;
  static const int kMaxBackgroundColor = 7;

  // Only for use as foreground colors.
  static const int kDarkGrey = 8;
  static const int kLightBlue = 9;
  static const int kLightGreen = 10;
  static const int kLightCyan = 11;
  static const int kLightRed = 12;
  static const int kLightMagenta = 13;
  static const int kYellow = 14;
  static const int kWhite = 15;

  FrameBuffer(char* fb, IoPorts* io);

  void Clear();

  void MoveCursor(int x, int y);

  void WriteCell(int x, int y, char c, int fg, int bg);
  void CopyCell(int dst_x, int dst_y, int src_x, int src_y);

  void WriteString(const char* str, int fg, int bg);

  void ScrollTo(int line);

private:
  void WriteChar(char c, int fg, int bg);

  char* fb_;
  IoPorts* io_;
  int x_ = 0, y_ = 0;
};

#endif  // framebuffer_h