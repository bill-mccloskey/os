#include <new>

#include "allocator.h"
#include "elf.h"
#include "frame_allocator.h"
#include "framebuffer.h"
#include "interrupts.h"
#include "io.h"
#include "lazy_global.h"
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

class ElfLoaderVisitor : public ElfVisitor {
public:
  ElfLoaderVisitor(PageTableManager* tables) : tables_(tables) {}

  void LoadSegment(int flags, const char* data, size_t size, virt_addr_t load_addr, size_t load_size) override {
    g_serial->Printf("  Would load segment (flags=%d) at %p, size=%d/%d\n", flags, load_addr, (int)size, (int)load_size);

    assert_le(size, load_size);

    PageAttributes attrs;

    phys_addr_t phys_start = VirtualToPhysical(reinterpret_cast<virt_addr_t>(data));
    assert_eq(phys_start & (kPageSize - 1), 0);

    phys_addr_t phys_end = phys_start + size;
    phys_end = (phys_end + kPageSize - 1) & ~(kPageSize - 1);

    virt_addr_t virt_start = load_addr;
    virt_addr_t virt_end = virt_start + size;
    virt_end = (virt_end + kPageSize - 1) & ~(kPageSize - 1);
    tables_->Map(phys_start, phys_end, virt_start, virt_end, attrs);

    if (load_size == size) return;

    // Need to zero-initialize the rest.
    size_t remainder = load_size - (phys_end - phys_start);
    remainder = (remainder + kPageSize - 1) & ~(kPageSize - 1);

    for (size_t bytes = 0; bytes < remainder; bytes += kPageSize) {
      virt_start = virt_end;
      virt_end += kPageSize;

      phys_start = g_frame_allocator->AllocateFrame();
      phys_end = phys_start + kPageSize;
      memset(reinterpret_cast<void*>(PhysicalToVirtual(phys_start)), 0, kPageSize);

      tables_->Map(phys_start, phys_end, virt_start, virt_end, attrs);
    }
  }

private:
  PageTableManager* tables_;
};

class MultibootLoaderVisitor : public MultibootVisitor {
public:
  MultibootLoaderVisitor(PageTableManager* tables) : tables_(tables) {}

  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    virt_addr_t start_addr = PhysicalToVirtual(module_start);
    size_t size = module_end - module_start;
    const char* data = reinterpret_cast<const char*>(start_addr);
    ElfReader reader(data, size);

    ElfLoaderVisitor loader_visitor(tables_);
    reader.Read(&loader_visitor);

    // Map in a stack.
    const virt_addr_t kStackBase = virt_addr_t(0x7ffffffff000);
    phys_addr_t program_stack = g_frame_allocator->AllocateFrame();
    tables_->Map(program_stack, program_stack + kPageSize,
                 kStackBase, kStackBase + kPageSize,
                 PageAttributes());

    // Map kernel memory.
    const size_t kMaxRAMSize = 1 << 21;
    tables_->Map(0, kMaxRAMSize, kKernelVirtualStart, kKernelVirtualStart + kMaxRAMSize, PageAttributes());

    phys_addr_t thread_phys = g_frame_allocator->AllocateFrame();
    void* thread_mem = reinterpret_cast<void*>(PhysicalToVirtual(thread_phys));
    Thread* thread = new (thread_mem) Thread(reader.entry_point(), kStackBase + kPageSize,
                                             tables_->table_root(), 0);
    thread->Start();
  }

private:
  PageTableManager* tables_;
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

  MultibootMemoryMapVisitor mem_visitor(&frame_allocator.value());
  multiboot_reader.Read(&mem_visitor);

  PageTableManager tables;
  MultibootLoaderVisitor load_visitor(&tables);
  multiboot_reader.Read(&load_visitor);

  PageTableManager idle_tables;
  const size_t kMaxRAMSize = 1 << 21;
  idle_tables.Map(0, kMaxRAMSize, kKernelVirtualStart, kKernelVirtualStart + kMaxRAMSize, PageAttributes());

  const virt_addr_t kStackBase = virt_addr_t(0x7ffffffff000);
  phys_addr_t program_stack = g_frame_allocator->AllocateFrame();
  idle_tables.Map(program_stack, program_stack + kPageSize,
                  kStackBase, kStackBase + kPageSize,
                  PageAttributes());

  Thread idle_task(virt_addr_t(&IdleTask), kStackBase + kPageSize - 8, idle_tables.table_root(), 2, true);
  idle_task.Start();

  scheduler->Start();

  // TODO:
  // - Create an AddressSpace class. Perhaps it should be reference counted by Threads? It owns a PageTableManager.
  // - Have a way to destroy Threads, AddressSpaces, and PageManagers at the right times.
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
