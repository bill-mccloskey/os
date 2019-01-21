#include "base/assertions.h"
#include "base/output_stream.h"

#ifdef TASK_BUILD

#include "usr/system.h"

namespace {

class DebugOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
    SysWriteByte(c);
  }
};

} // anonymous namespace

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

#elif TEST_BUILD

#include <stdio.h>

void AssertionFailure(const char* filename, int line, const char* msg1, const char* msg2, const char* msg3, const char* msg4) {
  fprintf(stderr, "Assertion failure(%s:%d): %s", filename, line, msg1);
  if (msg2) {
    fprintf(stderr, "%s", msg2);
    if (msg3) {
      fprintf(stderr, "%s", msg3);
      if (msg4) {
        fprintf(stderr, "%s", msg4);
      }
    }
  }

  fprintf(stderr, "\n");
  for (;;) {}
}

#elif KERNEL_BUILD

#include "kernel/serial.h"

void AssertionFailure(const char* filename, int line, const char* msg1, const char* msg2, const char* msg3, const char* msg4) {
  g_serial->Printf("%s:%d: %s", filename, line, msg1);

  if (msg2) {
    g_serial->Printf("%s", msg2);
    if (msg3) {
      g_serial->Printf("%s", msg3);
      if (msg4) {
        g_serial->Printf("%s", msg4);
      }
    }
  }

  g_serial->Printf("\n");
  for (;;) {}
}

#else

#error "Unexpected build type"

#endif
