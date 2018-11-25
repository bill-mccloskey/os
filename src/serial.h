#ifndef serial_h
#define serial_h

#include "io.h"

class SerialPort {
public:
  SerialPort(IoPorts* io, int com = 1, int baud_divisor = 1);

  void WriteByte(char c);
  void WriteString(const char* s);
  void Printf(const char* fmt, ...);

private:
  bool IsTransmitFifoEmpty();

  int DataPort() const { return port_; }
  int FifoCommandPort() const { return port_ + 2; }
  int LineCommandPort() const { return port_ + 3; }
  int ModemCommandPort() const { return port_ + 4; }
  int LineStatusPort() const { return port_ + 5; }

  IoPorts* io_;
  int port_;
};

extern SerialPort* g_serial;

#endif  // serial_h
