#include "framebuffer.h"
#include "interrupts.h"
#include "io.h"
#include "protection.h"
#include "serial.h"
#include "types.h"

struct MultibootTag {
  uint32_t type;
  uint32_t size;
} __attribute__((packed));

struct MultibootHeader {
  uint32_t total_size;
  uint32_t reserved;
} __attribute__((packed));

extern "C" {

void ReadMultiboot(const MultibootHeader* multiboot_info) {
  serial_printf(SERIAL_COM1_BASE, "Multiboot size = %u\n", multiboot_info->total_size);

  const char* p = reinterpret_cast<const char*>(multiboot_info + 1);
  const char* end = reinterpret_cast<const char*>(multiboot_info) + multiboot_info->total_size;

  while (p < end) {
    const MultibootTag* tag = reinterpret_cast<const MultibootTag*>(p);
    serial_printf(SERIAL_COM1_BASE, "  tag type = %u, size = %u\n", tag->type, tag->size);

    // Next tag will always be 8-byte aligned.
    uint32_t size_with_padding = ((tag->size + 7) >> 3) << 3;
    p += size_with_padding;
  }
}

void kmain(const MultibootHeader* multiboot_info) {
  IoPorts io;
  FrameBuffer fb((char *) 0xB8000, &io);

  fb.Clear();

  fb.WriteString("Hello world\nHi \n", FrameBuffer::kBlack, FrameBuffer::kWhite);

  serial_init(SERIAL_COM1_BASE, 1);
  serial_printf(SERIAL_COM1_BASE, "Hello world\nThis is serial %d -- %p\n", sizeof(void*), fb);

  ReadMultiboot(multiboot_info);

  fb.WriteString("Serial done\n", FrameBuffer::kBlack, FrameBuffer::kWhite);

  VMEnv env;
  VM vm(&env);
  vm.Load();

  serial_printf(SERIAL_COM1_BASE, "Protection setup worked. Going to user mode.\n");

  asm("cli\n"
      "xchg %bx,%bx\n"
      "mov %rsp, %rax\n"
      "push $0x1b\n"
      "push %rax\n"
      "pushf\n"
      "push $0x13\n"
      "push $some_code\n"
      "iretq");

  asm("int $3");

  InterruptController intr(&io);

  intr.Init();
  intr.Mask(0, false);

  asm("sti");

  // Goals:
  // Get to user mode (CPL3) without actually being able to get back.
  // To do this, I need to create a user-mode code segment.
  // Then I need to do something elaborate to jump to it via the iret instruction.
  // I'll just be jumping to some assembly code I write myself.
  // I'll do so with interrupts disabled, and the code will just invoke the bochs debugger.

  for (;;) {
    asm("hlt");
  }

  //outb(0xf4, 0);
}

}
