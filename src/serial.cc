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

void SerialPort::OutputChar(char c) {
  while (!IsTransmitFifoEmpty()) {}

  io_->Out(DataPort(), c);
}
