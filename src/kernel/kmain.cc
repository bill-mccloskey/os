#include <new>

#include "allocator.h"
#include "elf.h"
#include "frame_allocator.h"
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

namespace {

class MultibootPrintVisitor : public MultibootVisitor {
  void StartTag(int type) override {
    g_serial->Printf("  tag type = %u\n", type);
  }

  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    g_serial->Printf("    module %s: %x to %x\n", label, module_start, module_end);
  }

  void StartMemoryMap() override {
    g_serial->Printf("    memory map\n");
  }

  void MemoryMapEntry(uint64_t base_addr, uint64_t length, MemoryMapEntryType type) override {
    g_serial->Printf("    entry @%p size=%x, type=%d\n", (void*)base_addr, (unsigned)length, (int)type);
  }

  void Framebuffer(uint64_t addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) override {
    g_serial->Printf("    framebuffer @%p w=%u h=%u, bpp=%u pitch=%u\n",
                     (void*)addr, width, height, bpp, pitch);
  }

  void FramebufferRGBInfo(uint8_t red_field_position, uint8_t red_mask,
                          uint8_t green_field_position, uint8_t green_mask,
                          uint8_t blue_field_position, uint8_t blue_mask) override {
    g_serial->Printf("    with RGB info: red:(%u,%u) green:(%u,%u) blue:(%u,%u)\n",
                     red_field_position, red_mask,
                     green_field_position, green_mask,
                     blue_field_position, blue_mask);
  }
};

class MultibootModuleVisitor : public MultibootVisitor {
public:
  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    if (module_start < module_start_) {
      module_start_ = module_start;
    }

    if (module_end >= module_end_) {
      module_end_ = module_end;
    }
  }

  phys_addr_t module_start() const { return module_start_; }
  phys_addr_t module_end() const { return module_end_; }

private:
  phys_addr_t module_start_ = UINTPTR_MAX;
  phys_addr_t module_end_ = 0;
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

}  // anonymous namespace

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
static LazyGlobal<InterruptController> interrupts;

static LazyGlobal<Allocator<AddressSpace>> address_space_allocator;
static LazyGlobal<Allocator<Thread>> thread_allocator;

extern "C" {

void kernel_physical_start();
void kernel_physical_end();

void kmain(const char* multiboot_info) {
  MultibootReader multiboot_reader(multiboot_info);

  IoPorts io;
  serial_port.emplace(&io);
  g_serial = &serial_port.value();

  g_serial->Printf("Kernel start = %p\n", (void *)kernel_physical_start);
  g_serial->Printf("Kernel end = %p\n", (void *)kernel_physical_end);

  MultibootPrintVisitor print_visitor;
  multiboot_reader.Read(&print_visitor);

  MultibootModuleVisitor module_visitor;
  multiboot_reader.Read(&module_visitor);

  frame_allocator.emplace(phys_addr_t(kernel_physical_start), phys_addr_t(kernel_physical_end),
                          module_visitor.module_start(), module_visitor.module_end());
  g_frame_allocator = &frame_allocator.value();

  MultibootMemoryMapVisitor mem_visitor(&frame_allocator.value());
  multiboot_reader.Read(&mem_visitor);

  address_space_allocator.emplace();
  g_address_space_allocator = &address_space_allocator.value();
  thread_allocator.emplace();
  g_thread_allocator = &thread_allocator.value();

  phys_addr_t syscall_stack_phys = g_frame_allocator->AllocateFrame();
  virt_addr_t syscall_stack_virt = PhysicalToVirtual(syscall_stack_phys);
  virt_addr_t syscall_stack_top = syscall_stack_virt + kPageSize - Scheduler::SysCallStackAdjustment();

  VMEnv env;
  vm.emplace(&env);
  vm->Load(syscall_stack_top);
  g_vm = &vm.value();

  g_serial->Printf("Protection setup worked. Going to user mode.\n");

  interrupts.emplace(&io);
  g_interrupts = &interrupts.value();
  interrupts->Init();

  // Disable the timer interrupt.
  interrupts->Mask(0, false);
  interrupts->Mask(1, false);

  scheduler.emplace(syscall_stack_top);
  g_scheduler = &scheduler.value();

  LoadModules(multiboot_reader);

  RefPtr<AddressSpace> idle_as = new AddressSpace();
  Thread* idle_task = idle_as->CreateThread(virt_addr_t(&IdleTask), 2);
  idle_task->SetKernelThread();
  idle_task->Start();

  scheduler->Start();

  // TODO:
  // - Try to write drivers for keyboard, PS2 mouse, timer(?).
  // - Preemptive multitasking with timer interrupt.
  // - System calls to create threads and address spaces, map memory, etc.

  // This code exits qemu.
  //outb(0xf4, 0);
}

void __cxa_pure_virtual() {
  panic("Pure virtual function invoked");
}

}
