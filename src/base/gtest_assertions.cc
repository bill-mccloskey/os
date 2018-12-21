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
}
