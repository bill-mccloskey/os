#include <new>

#include "allocator.h"
#include "elf.h"
#include "frame_allocator.h"
#include "framebuffer.h"
#include "interrupts.h"
#include "io.h"
#include "lazy_global.h"
#include "loader.h"
#include "multiboot.h"
#include "page_tables.h"
#include "page_translation.h"
#include "protection.h"
#include "serial.h"
#include "string.h"
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

void IdleTask() {
  //asm("xchg %bx, %bx");

  for (;;) {
    asm("hlt");
  }
}

static LazyGlobal<SerialPort> serial_port;
static LazyGlobal<FrameAllocator> frame_allocator;
static LazyGlobal<VM> vm;
static LazyGlobal<Scheduler> scheduler;

static LazyGlobal<Allocator<AddressSpace>> address_space_allocator;
static LazyGlobal<Allocator<Thread>> thread_allocator;

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
  g_frame_allocator = &frame_allocator.value();

  MultibootMemoryMapVisitor mem_visitor(&frame_allocator.value());
  multiboot_reader.Read(&mem_visitor);

  address_space_allocator.emplace();
  g_address_space_allocator = &address_space_allocator.value();
  thread_allocator.emplace();
  g_thread_allocator = &thread_allocator.value();

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

  LoadModules(multiboot_reader);

  RefPtr<AddressSpace> idle_as = new AddressSpace(true);
  Thread* idle_task = idle_as->CreateThread(virt_addr_t(&IdleTask), 2);
  idle_task->Start();

  scheduler->Start();

  // TODO:
  // - Factor out loading of programs into a separate .cc file.
  // - Support large pages in page_tables.cc so the kernel can use one 1GB page instead of many 4K pages.
  // - Make it easier to add new system calls.
  // - Add system calls for IPC, waiting for interrupts.
  // - Try to write drivers for keyboard, PS2 mouse, timer(?).
  // - Preemptive multitasking with timer interrupt.
  // - System calls to create threads and address spaces, map memory, etc.

  // This code exits qemu.
  //outb(0xf4, 0);
}

}
