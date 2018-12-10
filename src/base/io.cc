#include "io.h"

#include "types.h"

void outb(unsigned short port, unsigned char data) {
  asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

unsigned char inb(unsigned short port) {
  uint8_t ret;
  asm volatile("inb %1, %0"
               : "=a"(ret)
               : "Nd"(port));
  return ret;
}

void IoPorts::Out(int port, unsigned char data) {
  outb(port, data);
}

int IoPorts::In(int port) {
  return inb(port);
}
