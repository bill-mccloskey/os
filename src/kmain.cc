#include "frame_allocator.h"
#include "framebuffer.h"
#include "interrupts.h"
#include "io.h"
#include "multiboot.h"
#include "protection.h"
#include "serial.h"
#include "types.h"

class MultibootPrintVisitor : public MultibootVisitor {
  void StartTag(int type) override {
    serial_printf(SERIAL_COM1_BASE, "  tag type = %u\n", type);
  }

  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    serial_printf(SERIAL_COM1_BASE, "    module %s: %p to %p\n", label, module_start, module_end);
  }

  void StartMemoryMap() override {
    serial_printf(SERIAL_COM1_BASE, "    memory map\n");
  }

  void MemoryMapEntry(uint64_t base_addr, uint64_t length, MemoryMapEntryType type) override {
    serial_printf(SERIAL_COM1_BASE, "    entry @%p size=%x, type=%d\n", base_addr, (unsigned)length, (int)type);
  }
};

class MultibootMemoryMapVisitor : public MultibootVisitor {
public:
  MultibootMemoryMapVisitor(FrameAllocator* frame_allocator) : frame_allocator_(frame_allocator) {}

  void MemoryMapEntry(uint64_t base_addr, uint64_t length, MemoryMapEntryType type) override {
    if (type != MemoryMapEntryType::kAvailableRAM) return;
    frame_allocator_->AddRegion(base_addr, base_addr + length);
  }

private:
  FrameAllocator* frame_allocator_;
};

extern "C" {

void kernel_physical_start();
void kernel_physical_end();

void kmain(const char* multiboot_info) {
  IoPorts io;
  FrameBuffer fb((char *) 0xB8000, &io);

  fb.Clear();

  fb.WriteString("Hello world\nHi \n", FrameBuffer::kBlack, FrameBuffer::kWhite);

  serial_init(SERIAL_COM1_BASE, 1);
  serial_printf(SERIAL_COM1_BASE, "Hello world\nThis is serial %d -- %p\n", sizeof(void*), fb);

  serial_printf(SERIAL_COM1_BASE, "Kernel start = %p\n", (void *)kernel_physical_start);
  serial_printf(SERIAL_COM1_BASE, "Kernel end = %p\n", (void *)kernel_physical_end);

  MultibootReader multiboot_reader(multiboot_info);

  MultibootPrintVisitor print_visitor;
  multiboot_reader.Read(&print_visitor);

  FrameAllocator allocator((phys_addr_t)(kernel_physical_start), phys_addr_t(kernel_physical_end));
  MultibootMemoryMapVisitor mem_visitor(&allocator);
  multiboot_reader.Read(&mem_visitor);

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
      "movabs $some_code, %rax\n"
      "push %rax\n"
      "iretq");

  asm("int $3");

  InterruptController intr(&io);

  intr.Init();
  intr.Mask(0, false);

  asm("sti");

  // Goals:
  // Then I'll try to set up multiple threads. Each one will run code from
  //   a GRUB module.
  // I'll implement cooperative multitasking between these threads.
  // They'll run in user-mode but have access to the entire address space.
  // Then I'll try to set up page tables to cover each process and whatever I
  //   need of the kernel. I'll switch when I switch processes.

  for (;;) {
    asm("hlt");
  }

  //outb(0xf4, 0);
}

}
