#include "loader.h"

#include "address_space.h"
#include "assertions.h"
#include "elf.h"
#include "frame_allocator.h"
#include "kernel_module.h"
#include "page_translation.h"
#include "serial.h"
#include "thread.h"
#include "types.h"

class ElfLoaderVisitor : public ElfVisitor {
public:
  ElfLoaderVisitor(const RefPtr<AddressSpace>& as) : address_space_(as) {}

  void LoadSegment(int flags, const char* data, size_t size, virt_addr_t load_addr, size_t load_size) override {
    g_serial->Printf("  Loading segment (flags=%d) at %p, size=%d/%d\n", flags, (void*)load_addr, (int)size, (int)load_size);

    assert_le(size, load_size);

    PageAttributes attrs;

    attrs.set_writable(flags & kFlagWrite);
    attrs.set_no_execute(!(flags & kFlagExecute));

    phys_addr_t phys_start = VirtualToPhysical(reinterpret_cast<virt_addr_t>(data));
    phys_addr_t phys_end = phys_start + size;

    // Round phys_start down to page alignment. Round phys_end up to page alignment.
    phys_start = phys_start & ~(kPageSize - 1);
    phys_end = (phys_end + kPageSize - 1) & ~(kPageSize - 1);

    virt_addr_t virt_start = load_addr;
    virt_addr_t virt_end = virt_start + size;
    virt_start = virt_start & ~(kPageSize - 1);
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

static bool StringIs(const char* start, const char* end, const char* cmp) {
  const char* p = start;
  while (p < end) {
    if (*p != *cmp) return false;
    p++;
    cmp++;
  }
  return true;
}

static uint64_t ParseNum(int base, const char* start, const char* end) {
  uint64_t v = 0;
  while (start < end) {
    v *= base;

    if (*start >= '0' && *start <= '9') v |= *start - '0';
    else if (*start >= 'a' && *start <= 'f') v |= *start - 'a' + 10;
    else panic("Invalid address for map argument");

    start++;
  }

  return v;
}

class MultibootDataVisitor : public MultibootVisitor {
public:
  void Framebuffer(uint64_t addr, uint32_t pitch, uint32_t width, uint32_t height, uint8_t bpp) override {
    framebuffer_start_ = addr;
    framebuffer_end_ = addr + width * pitch;

    module_data_.framebuffer.addr = addr;
    module_data_.framebuffer.pitch = pitch;
    module_data_.framebuffer.width = width;
    module_data_.framebuffer.height = height;
    module_data_.framebuffer.bpp = bpp;
  }

  phys_addr_t framebuffer_start() const { return framebuffer_start_; }
  phys_addr_t framebuffer_end() const { return framebuffer_end_; }

  KernelModuleData& module_data() { return module_data_; }

private:
  KernelModuleData module_data_;

  phys_addr_t framebuffer_start_ = 0;
  phys_addr_t framebuffer_end_ = 0;
};

static void ParseArguments(const char* args, MultibootDataVisitor* data, const RefPtr<AddressSpace>& as, Thread* thread) {
  const char* p = args;
  while (*p) {
    const char* key = p;
    const char* end_key = p;
    while (*end_key && *end_key != '=') end_key++;
    if (*end_key != '=') panic("Invalid command line argument");

    const char* value = end_key + 1;
    const char* end_value = value;
    while (*end_value && *end_value != ' ') end_value++;

    if (StringIs(key, end_key, "map")) {
      const char* comma = value;
      while (comma < end_value && *comma != ',') comma++;
      if (comma == end_value) panic("Invalid memory range for map argument");

      phys_addr_t start = ParseNum(16, value, comma);
      phys_addr_t end = ParseNum(16, comma + 1, end_value);
      g_serial->Printf("Mapping to userspace: %p to %p\n", (void*)start, (void*)end);
      as->Map(start, end, start, end, PageAttributes());
    } else if (StringIs(key, end_key, "videomap")) {
      if (StringIs(value, end_value, "true")) {
        as->Map(data->framebuffer_start(), data->framebuffer_end(),
                data->framebuffer_start(), data->framebuffer_end(),
                PageAttributes());
      } else if (StringIs(value, end_value, "false")) {
        // Do nothing. This is the default.
      } else {
        panic("Unrecognized videomap argument");
      }
    } else if (StringIs(key, end_key, "allow_io")) {
      if (StringIs(value, end_value, "true")) {
        thread->AllowIo();
      } else if (StringIs(value, end_value, "false")) {
        // Do nothing. This is the default.
      } else {
        panic("Unrecognized allow_io argument");
      }
    } else if (StringIs(key, end_key, "tid")) {
      int tid = ParseNum(10, value, end_value);
      thread->set_id(tid);
    } else {
      panic("Unrecognized command line argument");
    }

    p = end_value;
    while (*p == ' ') p++;
  }
}

class MultibootLoaderVisitor : public MultibootVisitor {
public:
  MultibootLoaderVisitor(MultibootDataVisitor* data) : data_(data) {}

  void Module(const char* args, uint32_t module_start, uint32_t module_end) override {
    g_serial->Printf("Loading module %s\n", args);

    RefPtr<AddressSpace> as = new AddressSpace();

    virt_addr_t start_addr = PhysicalToVirtual(module_start);
    size_t size = module_end - module_start;
    const char* data = reinterpret_cast<const char*>(start_addr);
    ElfReader reader(data, size);

    ElfLoaderVisitor loader_visitor(as);
    reader.Read(&loader_visitor);

    Thread* thread = as->CreateThread(reader.entry_point(), 0,
                                      &data_->module_data(), sizeof(KernelModuleData));

    ParseArguments(args, data_, as, thread);

    thread->Start();
  }

private:
  MultibootDataVisitor* data_;
};

void LoadModules(const MultibootReader& multiboot_reader) {
  MultibootDataVisitor data_visitor;
  multiboot_reader.Read(&data_visitor);

  MultibootLoaderVisitor load_visitor(&data_visitor);
  multiboot_reader.Read(&load_visitor);
}
