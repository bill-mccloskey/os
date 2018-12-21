#include "assertions.h"
#include "output_stream.h"

extern "C" {
void SysWriteByte(char c);
}

class DebugOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
    SysWriteByte(c);
  }
};

void AssertionFailure(const char* filename, int line, const char* msg1, const char* msg2, const char* msg3, const char* msg4) {
  DebugOutputStream os;
  os.Printf("%s:%d: %s", filename, line, msg1);

  if (msg2) {
    os.Printf("%s", msg2);
    if (msg3) {
      os.Printf("%s", msg3);
      if (msg4) {
        os.Printf("%s", msg4);
      }
    }
  }

  os.Printf("\n");
  for (;;) {}
}
