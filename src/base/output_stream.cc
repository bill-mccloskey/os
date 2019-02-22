#include "output_stream.h"

#include <stdarg.h>

#ifdef TEST_BUILD
#include <stdio.h>
#elif KERNEL_BUILD
#include "kernel/serial.h"
#elif TASK_BUILD
#include "usr/system.h"
#endif

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

        case 'c': {
          int c = va_arg(args, int);
          OutputChar(c);
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

OutputStream& OutputStream::operator<<(const char* s) {
  OutputString(s);
  return *this;
}

OutputStream& OutputStream::operator<<(int x) {
  char buf[32];
  SignedNumToString(x, buf, 10);
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(unsigned int x) {
  char buf[32];
  UnsignedNumToString(x, buf, 10);
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(long x) {
  char buf[32];
  SignedNumToString(x, buf, 10);
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(unsigned long x) {
  char buf[32];
  UnsignedNumToString(x, buf, 10);
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(long long x) {
  char buf[32];
  SignedNumToString(x, buf, 10);
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(unsigned long long x) {
  char buf[32];
  UnsignedNumToString(x, buf, 10);
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(void* x) {
  char buf[32];
  UnsignedNumToString((unsigned long)x, buf, 16);
  OutputString("0x");
  OutputString(buf);
  return *this;
}

OutputStream& OutputStream::operator<<(bool x) {
  if (x) {
    OutputString("true");
  } else {
    OutputString("false");
  }
  return *this;
}

namespace {

class LogOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
#ifdef TASK_BUILD
    SysWriteByte(c);
#elif KERNEL_BUILD
    g_serial->OutputChar(c);
#elif TEST_BUILD
    putchar(c);
    fflush(stdout);
#endif
  }
};

class NullOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {}
};

}

TerminatingOutputStream Log(const char* filename, int line, LogLevel level) {
  if (level == DEBUG) {
    static NullOutputStream stream;
    TerminatingOutputStream tmp(stream);
    return tmp;
  } else {
    static LogOutputStream stream;
    TerminatingOutputStream tmp(stream);
    tmp.Printf("%s:%d: ", filename, line);
    return tmp;
  }
}
