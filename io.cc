#include "io.h"

void IoPorts::Out(int port, unsigned char data) {
  outb(port, data);
}

int IoPorts::In(int port) {
  return inb(port);
}
