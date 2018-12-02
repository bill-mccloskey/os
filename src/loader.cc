#include "loader.h"

#include "address_space.h"
#include "assertions.h"
#include "elf.h"
#include "frame_allocator.h"
#include "page_translation.h"
#include "serial.h"
#include "thread.h"
#include "types.h"

class ElfLoaderVisitor : public ElfVisitor {
public:
  ElfLoaderVisitor(const RefPtr<AddressSpace>& as) : address_space_(as) {}

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
    address_space_->Map(phys_start, phys_end, virt_start, virt_end, attrs);

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

      address_space_->Map(phys_start, phys_end, virt_start, virt_end, attrs);
    }
  }

private:
  RefPtr<AddressSpace> address_space_;
};

class MultibootLoaderVisitor : public MultibootVisitor {
public:
  MultibootLoaderVisitor() {}

  void Module(const char* label, uint32_t module_start, uint32_t module_end) override {
    RefPtr<AddressSpace> as = new AddressSpace();

    virt_addr_t start_addr = PhysicalToVirtual(module_start);
    size_t size = module_end - module_start;
    const char* data = reinterpret_cast<const char*>(start_addr);
    ElfReader reader(data, size);

    ElfLoaderVisitor loader_visitor(as);
    reader.Read(&loader_visitor);

    Thread* thread = as->CreateThread(reader.entry_point(), 0);
    thread->Start();
  }
};

void LoadModules(const MultibootReader& multiboot_reader) {
  MultibootLoaderVisitor load_visitor;
  multiboot_reader.Read(&load_visitor);
}
