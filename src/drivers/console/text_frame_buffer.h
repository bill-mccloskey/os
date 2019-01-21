#ifndef text_frame_buffer_h
#define text_frame_buffer_h

#include "base/io.h"
#include "drivers/console/abstract_frame_buffer.h"

class TextFrameBuffer : public AbstractFrameBuffer {
public:
  static const int kWidth = 80;
  static const int kHeight = 25;

  int Width() override { return kWidth; }
  int Height() override { return kHeight; }

  TextFrameBuffer(char* fb, IoPorts* io);

  void MoveCursor(int x, int y) override;

  void WriteCell(int x, int y, char c, Color fg, Color bg) override;
  void CopyCell(int dst_x, int dst_y, int src_x, int src_y) override;

  void ScrollTo(int line) override;

private:
  int SelectColor(Color color, bool is_fg);

  char* fb_;
  IoPorts* io_;
};

#endif  // text_frame_buffer_h
