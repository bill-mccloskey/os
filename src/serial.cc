#include "serial.h"

#include <stdarg.h>

static const int kSerialCom1 = 0x3f8;
static const int kLineEnableDlab = 0x80;

SerialPort* g_serial;

SerialPort::SerialPort(IoPorts* io, int com, int baud_divisor)
  : io_(io), port_(com - 1 + kSerialCom1) {
  // Set baud rate to 115200 / divisor.
  io_->Out(LineCommandPort(), kLineEnableDlab);
  io_->Out(DataPort(), (baud_divisor >> 8) & 0xff);
  io_->Out(DataPort(), baud_divisor & 0xff);

  // 8 bits, no parity, one stop bit, no break.
  io_->Out(LineCommandPort(), 0x03);

  // Enable FIFO, clear them, with 14-byte threshold.
  io_->Out(FifoCommandPort(), 0xc7);

  // RTS=1, DTS=1 (flow control enabled).
  io_->Out(ModemCommandPort(), 0x03);
}

bool SerialPort::IsTransmitFifoEmpty() {
  /* 0x20 = 0010 0000 */
  return io_->In(LineStatusPort()) & 0x20;
}

void SerialPort::WriteByte(char c) {
  while (!IsTransmitFifoEmpty()) {}

  io_->Out(DataPort(), c);
}

void SerialPort::WriteString(const char* s) {
  for (int i = 0; s[i]; i++) {
    WriteByte(s[i]);
  }
}

static void num_to_string(long n, char* buf, int base) {
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

static void unsigned_to_string(unsigned long n, char* buf, int base) {
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

void SerialPort::Printf(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 'd': {
          int x = va_arg(args, int);
          char buf[32];
          num_to_string(x, buf, 10);
          WriteString(buf);
          break;
        }

        case 'u': {
          unsigned x = va_arg(args, unsigned);
          char buf[32];
          unsigned_to_string(x, buf, 10);
          WriteString(buf);
          break;
        }

        case 'p': {
          void* x = va_arg(args, void*);
          char buf[32];
          unsigned_to_string((unsigned long)x, buf, 16);
          WriteString("0x");
          WriteString(buf);
          break;
        }

        case 'x': {
          unsigned x = va_arg(args, unsigned);
          char buf[32];
          unsigned_to_string(x, buf, 16);
          WriteString("0x");
          WriteString(buf);
          break;
        }

        case 's': {
          char* s = va_arg(args, char*);
          WriteString(s);
          break;
        }

        default: {
          WriteByte('%');
          WriteByte(*fmt);
          break;
        }
      }
      fmt++;
    } else {
      WriteByte(*fmt);
      fmt++;
    }
  }

  va_end(args);
}
