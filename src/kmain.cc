#include "elf.h"
#include "frame_allocator.h"
#include "framebuffer.h"
#include "interrupts.h"
#include "io.h"
#include "lazy_global.h"
#include "multiboot.h"
#include "page_translation.h"
#include "protection.h"
#include "serial.h"
#include "thread.h"
#include "types.h"

class MultibootPrintVisitor : public MultibootVisitor {
  void StartTag(int type) override {
    g_serial->Printf("  tag type = %u\n", type);
  }

  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    g_serial->Printf("    module %s: %p to %p\n", label, module_start, module_end);
  }

  void StartMemoryMap() override {
    g_serial->Printf("    memory map\n");
  }

  void MemoryMapEntry(uint64_t base_addr, uint64_t length, MemoryMapEntryType type) override {
    g_serial->Printf("    entry @%p size=%x, type=%d\n", base_addr, (unsigned)length, (int)type);
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

class ElfLoaderVisitor : public ElfVisitor {
public:
  void LoadSegment(int flags, const char* data, size_t size, virt_addr_t load_addr, size_t load_size) override {
    g_serial->Printf("  Would load segment (flags=%d) at %p, size=%d\n", flags, load_addr, (int)size);
  }
};

class MultibootLoaderVisitor : public MultibootVisitor {
public:
  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    virt_addr_t start_addr = PhysicalToVirtual(module_start);
    size_t size = module_end - module_start;
    const char* data = reinterpret_cast<const char*>(start_addr);
    ElfReader reader(data, size);

    ElfLoaderVisitor loader_visitor;
    reader.Read(&loader_visitor);
  }
};

extern "C" {
uint64_t syscall(uint64_t number, uint64_t arg1 = 0, uint64_t arg2 = 0, uint64_t arg3 = 0,
                 uint64_t arg4 = 0, uint64_t arg5 = 0);
}

void UserThread1() {
  //asm("xchg %bx, %bx");

  syscall(1, 'H');
  syscall(1, 'i');
  syscall(1, '1');
  syscall(1, '!');
  syscall(1, '\n');

  syscall(2);

  syscall(1, 'B');
  syscall(1, 'y');
  syscall(1, 'e');
  syscall(1, '1');
  syscall(1, '\n');

  syscall(2);

  //asm("xchg %bx, %bx");

  for (;;) {}
}

void UserThread2() {
  //asm("xchg %bx, %bx");

  syscall(1, 'H');
  syscall(1, 'i');
  syscall(1, '2');
  syscall(1, '!');
  syscall(1, '\n');

  syscall(2);

  syscall(1, 'B');
  syscall(1, 'y');
  syscall(1, 'e');
  syscall(1, '2');
  syscall(1, '\n');

  //asm("xchg %bx, %bx");

  for (;;) {}
}

static LazyGlobal<SerialPort> serial_port;
static LazyGlobal<FrameAllocator> frame_allocator;
static LazyGlobal<VM> vm;
static LazyGlobal<Scheduler> scheduler;

extern "C" {

void kernel_physical_start();
void kernel_physical_end();

void kmain(const char* multiboot_info) {
  IoPorts io;
  FrameBuffer fb((char*)0xb8000, &io);

  fb.Clear();

  fb.WriteString("Hello world\nHi \n", FrameBuffer::kBlack, FrameBuffer::kWhite);

  serial_port.emplace(&io);
  g_serial = &serial_port.value();

  g_serial->Printf("Hello world\nThis is serial %d -- %p\n", sizeof(void*), &fb);

  g_serial->Printf("Kernel start = %p\n", (void *)kernel_physical_start);
  g_serial->Printf("Kernel end = %p\n", (void *)kernel_physical_end);

  MultibootReader multiboot_reader(multiboot_info);

  MultibootPrintVisitor print_visitor;
  multiboot_reader.Read(&print_visitor);

  frame_allocator.emplace(phys_addr_t(kernel_physical_start), phys_addr_t(kernel_physical_end));
  MultibootMemoryMapVisitor mem_visitor(&frame_allocator.value());
  multiboot_reader.Read(&mem_visitor);

  MultibootLoaderVisitor load_visitor;
  multiboot_reader.Read(&load_visitor);

  g_frame_allocator = &frame_allocator.value();

  fb.WriteString("Serial done\n", FrameBuffer::kBlack, FrameBuffer::kWhite);

  VMEnv env;
  vm.emplace(&env);
  vm->Load();
  g_vm = &vm.value();

  g_serial->Printf("Protection setup worked. Going to user mode.\n");

  InterruptController intr(&io);

  intr.Init();

  // Disable the timer interrupt.
  intr.Mask(0, false);

  scheduler.emplace();
  g_scheduler = &scheduler.value();

  Thread user_thread1(virt_addr_t(&UserThread1), 0);
  user_thread1.Start();

  Thread user_thread2(virt_addr_t(&UserThread2), 0);
  user_thread2.Start();

  scheduler->Start();

  // This code exits qemu.
  //outb(0xf4, 0);
}

}
