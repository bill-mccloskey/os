#ifndef raster_frame_buffer_h
#define raster_frame_buffer_h

#include "abstract_frame_buffer.h"
#include "kernel_module.h"

class RasterFrameBuffer : public AbstractFrameBuffer {
public:
  enum FontName {
    VGA,
    Fixed
  };
  RasterFrameBuffer(FrameBufferData* fb, FontName font);

  int Width() override;
  int Height() override;

  void MoveCursor(int x, int y) override;

  void WriteCell(int x, int y, char c, Color fg, Color bg) override;
  void CopyCell(int dst_x, int dst_y, int src_x, int src_y) override;

  void Clear() override;

  void ScrollTo(int line) override;

private:
  unsigned char* GlyphData(int ch);
  void SelectColor(Color color, bool is_fg, int* red, int* green, int* blue);

  static const int kGlyphWidth = 8;

  FrameBufferData* fb_;
  unsigned char* font_;
  int glyph_height_;
};

#endif  // raster_frame_buffer_h
