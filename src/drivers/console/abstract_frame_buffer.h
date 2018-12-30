#ifndef abstract_frame_buffer_h
#define abstract_frame_buffer_h

class AbstractFrameBuffer {
public:
  enum Color {
    kBlack,
    kBlue,
    kGreen,
    kCyan,
    kRed,
    kMagenta,
    kBrown,
    kLightGray,
    kYellow,
    kWhite,
    kDefault
  };

  virtual void Clear();

  virtual void MoveCursor(int x, int y) = 0;

  virtual void WriteCell(int x, int y, char c, Color fg, Color bg) = 0;
  virtual void CopyCell(int dst_x, int dst_y, int src_x, int src_y) = 0;

  virtual void WriteString(const char* str, Color fg, Color bg);

  virtual void ScrollTo(int line) = 0;

  virtual int Width() = 0;
  virtual int Height() = 0;

private:
  void WriteChar(char c, Color fg, Color bg);

  int x_ = 0, y_ = 0;
};

#endif  // abstract_frame_buffer_h
