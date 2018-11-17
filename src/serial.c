#include "io.h" /* io.h is implement in the section "Moving the cursor" */

#include <stdarg.h>

/* The I/O ports */

/* All the I/O ports are calculated relative to the data port. This is because
 * all serial ports (COM1, COM2, COM3, COM4) have their ports in the same
 * order, but they start at different values.
 */

#define SERIAL_COM1_BASE                0x3F8      /* COM1 base port */

#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_FIFO_COMMAND_PORT(base)  (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

/* The I/O port commands */

/* SERIAL_LINE_ENABLE_DLAB:
 * Tells the serial port to expect first the highest 8 bits on the data port,
 * then the lowest 8 bits will follow
 */
#define SERIAL_LINE_ENABLE_DLAB         0x80

void serial_init(unsigned short com, unsigned short divisor) {
  // Set baud rate to 115200 / divisor.
  outb(SERIAL_LINE_COMMAND_PORT(com),
       SERIAL_LINE_ENABLE_DLAB);
  outb(SERIAL_DATA_PORT(com),
       (divisor >> 8) & 0x00FF);
  outb(SERIAL_DATA_PORT(com),
       divisor & 0x00FF);

  // 8 bits, no parity, one stop bit, no break.
  outb(SERIAL_LINE_COMMAND_PORT(com), 0x03);

  // Enable FIFO, clear them, with 14-byte threshold.
  outb(SERIAL_FIFO_COMMAND_PORT(com), 0xC7);

  // RTS=1, DTS=1 (flow control enabled).
  outb(SERIAL_MODEM_COMMAND_PORT(com), 0x03);
}

/** serial_is_transmit_fifo_empty:
 *  Checks whether the transmit FIFO queue is empty or not for the given COM
 *  port.
 *
 *  @param  com The COM port
 *  @return 0 if the transmit FIFO queue is not empty
 *          1 if the transmit FIFO queue is empty
 */
int serial_is_transmit_fifo_empty(unsigned int com)
{
  /* 0x20 = 0010 0000 */
  return inb(SERIAL_LINE_STATUS_PORT(com)) & 0x20;
}

void serial_write_byte(unsigned int com, char c) {
  while (!serial_is_transmit_fifo_empty(com)) {}

  outb(com, c);
}

void serial_write(unsigned int com, const char* s) {
  for (int i = 0; s[i]; i++) {
    serial_write_byte(com, s[i]);
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

void serial_printf(unsigned int com, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  serial_write(com, "SERIAL - ");

  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      switch (*fmt) {
        case 'd': {
          int x = va_arg(args, int);
          char buf[32];
          num_to_string(x, buf, 10);
          serial_write(com, buf);
          break;
        }

        case 'u': {
          unsigned x = va_arg(args, unsigned);
          char buf[32];
          unsigned_to_string(x, buf, 10);
          serial_write(com, buf);
          break;
        }

        case 'p': {
          void* x = va_arg(args, void*);
          char buf[32];
          unsigned_to_string((unsigned long)x, buf, 16);
          serial_write(com, "0x");
          serial_write(com, buf);
          break;
        }

        case 'x': {
          unsigned x = va_arg(args, unsigned);
          char buf[32];
          unsigned_to_string(x, buf, 16);
          serial_write(com, "0x");
          serial_write(com, buf);
          break;
        }

        case 's': {
          char* s = va_arg(args, char*);
          serial_write(com, s);
          break;
        }

        default: {
          serial_write_byte(com, '%');
          serial_write_byte(com, *fmt);
          break;
        }
      }
      fmt++;
    } else {
      serial_write_byte(com, *fmt);
      fmt++;
    }
  }

  va_end(args);
}
