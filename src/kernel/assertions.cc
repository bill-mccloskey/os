#include "assertions.h"
#include "serial.h"

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
