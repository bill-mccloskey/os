#include "output_stream.h"

#include <stdarg.h>

void OutputStream::OutputString(const char* s) {
  for (int i = 0; s[i]; i++) {
    OutputChar(s[i]);
  }
}

static void SignedNumToString(long n, char* buf, int base) {
  long digits = 0;
  long x = n;
  while (x != 0) {
    x /= base;
    digits++;
  }

  if (n == 0) {
    buf[0] = '0';
    buf[1] = 0x00;
    return;
  }

  if (n < 0) {
    buf[0] = '-';
    buf++;
    n = -n;
  }

  buf[digits] = 0x00;
  while (n != 0) {
    int digit = n % base;
    if (digit <= 9) {
      buf[--digits] = digit + '0';
    } else {
      buf[--digits] = digit - 10 + 'a';
    }
    n /= base;
  }
}

static void UnsignedNumToString(unsigned long n, char* buf, int base) {
  unsigned long digits = 0;
  unsigned long x = n;
  while (x != 0) {
    x /= base;
    digits++;
  }

  if (n == 0) {
    buf[0] = '0';
    buf[1] = 0x00;
    return;
  }

  buf[digits] = 0x00;
  while (n != 0) {
    int digit = n % base;
    if (digit <= 9) {
      buf[--digits] = digit + '0';
    } else {
      buf[--digits] = digit - 10 + 'a';
    }
    n /= base;
  }
}

void OutputStream::Printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 'd': {
          int x = va_arg(args, int);
          char buf[32];
          SignedNumToString(x, buf, 10);
          OutputString(buf);
          break;
        }

        case 'u': {
          unsigned x = va_arg(args, unsigned);
          char buf[32];
          UnsignedNumToString(x, buf, 10);
          OutputString(buf);
          break;
        }

        case 'p': {
          void* x = va_arg(args, void*);
          char buf[32];
          UnsignedNumToString((unsigned long)x, buf, 16);
          OutputString("0x");
          OutputString(buf);
          break;
        }

        case 'x': {
          unsigned x = va_arg(args, unsigned);
          char buf[32];
          UnsignedNumToString(x, buf, 16);
          OutputString("0x");
          OutputString(buf);
          break;
        }

        case 's': {
          char* s = va_arg(args, char*);
          OutputString(s);
          break;
        }

        default: {
          OutputChar('%');
          OutputChar(*fmt);
          break;
        }
      }
      fmt++;
    } else {
      OutputChar(*fmt);
      fmt++;
    }
  }

  va_end(args);
}
