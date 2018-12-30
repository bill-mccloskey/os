#include "abstract_frame_buffer.h"

void AbstractFrameBuffer::Clear() {
  for (int x = 0; x < Width(); x++) {
    for (int y = 0; y < Height(); y++) {
      WriteCell(x, y, ' ', kDefault, kDefault);
    }
  }
}

void AbstractFrameBuffer::WriteChar(char c, Color fg, Color bg) {
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

void AbstractFrameBuffer::WriteString(const char* str, Color fg, Color bg) {
  for (int i = 0; str[i]; i++) {
    WriteChar(str[i], fg, bg);
  }
}
