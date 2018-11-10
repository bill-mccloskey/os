#include "framebuffer.h"
#include "io.h"
#include "protection.h"
#include "serial.h"

extern "C" {

void kmain() {
  IoPorts io;
  FrameBuffer fb((char *) 0xB8000, &io);

  fb.Clear();

  fb.WriteString("Hello world\nHi \n", FrameBuffer::kBlack, FrameBuffer::kWhite);

  serial_init(SERIAL_COM1_BASE, 1);
  serial_printf(SERIAL_COM1_BASE, "Hello world\nThis is serial %d -- %p\n", sizeof(void*), fb);

  VMEnv env;
  VM vm(&env);
  vm.Load();

  serial_printf(SERIAL_COM1_BASE, "Protection setup worked?\n");

  outb(0xf4, 0);
}

}
