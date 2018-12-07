#include "protection.h"

#include "frame_allocator.h"
#include "page_translation.h"
#include "serial.h"
#include "string.h"
#include "thread.h"

VM* g_vm;

namespace {

struct SegmentSelectorLayout {
  uint8_t requested_privilege_level : 2;
  bool is_ldt : 1;
  uint16_t index : 13;
} __attribute__((packed));

// The only fields used in long mode are:
// - is_code_or_data
// - descriptor_privilege_level
// - present
// - segment_long
// - op_size_32_bits
//
// When segment_long=1, op_size_32_bits should be 0.
struct SegmentDescriptorLayout {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_middle;

  uint8_t type : 4;
  bool non_system_segment : 1;
  uint8_t descriptor_privilege_level : 2;
  bool present : 1;

  uint8_t limit_high : 4;
  bool reserved : 1;
  bool segment_long : 1;
  bool op_size_32_bits : 1;
  bool page_granularity : 1;

  uint8_t base_high;
} __attribute__((packed));

struct SystemDescriptorLayout {
  SegmentDescriptorLayout desc;
  uint32_t base_very_high;

  // Some of these bits must be zero. Just make them all zero.
  uint32_t reserved;
} __attribute__((packed));

// is_trap means interrupts are not disabled during the handler.
struct InterruptDescriptorLayout {
  uint16_t offset_low;
  SegmentSelector::Storage segment_selector;

  uint8_t interrupt_stack : 3;
  uint8_t reserved : 5;
  bool is_trap : 1;
  uint8_t must_be_one : 3;
  uint8_t must_be_zero : 1;
  uint8_t descriptor_privilege_level : 2;
  bool present : 1;

  uint16_t offset_middle;
  uint32_t offset_high;
} __attribute__((packed));

struct TaskStateSegmentLayout {
  uint32_t reserved1;
  uint64_t privilege_stack_table[3];
  uint64_t reserved2;
  uint64_t interrupt_stack_table[7];
  uint64_t reserved3;
  uint16_t reserved4;
  uint16_t io_map_base_address;
} __attribute__((packed));

} // namespace

SegmentSelector::Storage SegmentSelector::Serialize() const {
  SegmentSelectorLayout d = {};

  d.requested_privilege_level = requested_priv_;
  d.is_ldt = is_ldt_;
  d.index = selector_;

  Storage storage;
  memcpy(&storage, &d, sizeof(d));
  return storage;
}

int SegmentDescriptor::Serialize(uint8_t* storage) const {
  const int kBaseMiddleShift = 16;
  const int kBaseHighShift = 24;
  const int kBaseVeryHighShift = 32;

  const int kByteGranularityMax = 0xfffff;
  const int kLimitHighShift = 16;
  const int kPageGranularityShift = 12;

  SegmentDescriptorLayout d = {};

  d.base_low = uint16_t(base_);
  d.base_middle = uint8_t(base_ >> kBaseMiddleShift);
  d.base_high = uint8_t(base_ >> kBaseHighShift);

  d.descriptor_privilege_level = required_priv_;
  d.present = true;
  d.segment_long = segment_long_;
  d.op_size_32_bits = !segment_long_;
  d.non_system_segment = type_ == kCodeSegment || type_ == kDataSegment;

  if (last_byte_index_ > kByteGranularityMax) {
    d.page_granularity = true;

    size_t limit = last_byte_index_ >> kPageGranularityShift;
    d.limit_low = uint16_t(limit);
    d.limit_high = limit >> kLimitHighShift;
  } else {
    d.page_granularity = false;
    d.limit_low = uint16_t(last_byte_index_);
    d.limit_high = last_byte_index_ >> kLimitHighShift;
  }

  if (type_ == kTaskStateSegment) {
    d.type = 0b1001;

    SystemDescriptorLayout system_layout = {d, 0, 0};
    system_layout.base_very_high = uint32_t(base_ >> kBaseVeryHighShift);

    memcpy(storage, &system_layout, sizeof(system_layout));
    return 2;
  } else {
    if (type_ == kCodeSegment) {
      d.type = 0b1010;
    } else if (type_ == kDataSegment) {
      d.type = 0b0011;
    }

    memcpy(storage, &d, sizeof(d));
    return 1;
  }
}

void InterruptDescriptor::Serialize(Storage storage) const {
  InterruptDescriptorLayout d = {};

  d.offset_low = uint16_t(offset_);
  d.segment_selector = segment_.Serialize();
  d.interrupt_stack = interrupt_stack_;
  d.is_trap = !disable_interrupts_;
  d.must_be_one = 0x7;
  d.descriptor_privilege_level = required_priv_;
  d.present = true;
  d.offset_middle = uint16_t(offset_ >> 16);
  d.offset_high = uint32_t(offset_ >> 32);

  memcpy(storage, &d, sizeof(d));
}

void TaskStateSegment::Serialize(Storage storage) const {
  TaskStateSegmentLayout d = {};

  for (int i = 0; i < kNumPrivilegedStacks; i++) {
    d.privilege_stack_table[i] = privileged_stacks_[i];
  }

  for (int i = 0; i < kNumInterruptStacks; i++) {
    d.interrupt_stack_table[i] = interrupt_stacks_[i];
  }

  d.io_map_base_address = sizeof(Storage);

  memcpy(storage, &d, sizeof(d));
}

void VMEnv::LoadGDT(virt_addr_t base, size_t size) {
  struct Descriptor {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed));

  Descriptor desc __attribute__((aligned(8)));

  desc.base = base;
  desc.limit = size - 1;

  asm("lgdt (%0)" : : "r"(&desc));
}

void VMEnv::LoadIDT(virt_addr_t base, size_t size) {
  struct Descriptor {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed));

  Descriptor desc __attribute__((aligned(8)));

  desc.base = base;
  desc.limit = size - 1;

  asm("lidt (%0)" : : "r"(&desc));
}

void VMEnv::LoadTSS(const SegmentSelector& selector) {
  SegmentSelector::Storage storage = selector.Serialize();
  asm("mov %0, %%ax\n"
      "ltr %%ax" : : "r"(storage));
}

VM::VM(VMEnv* env) : env_(env) {
}

void VM::AddGDTEntry(int number, const SegmentDescriptor& segdesc) {
  segdesc.Serialize(gdt_[number]);
}

void VM::AddIDTEntry(int number, const InterruptDescriptor& desc) {
  desc.Serialize(idt_[number]);
}

struct InterruptHandlerEntry {
  virt_addr_t handler;
  int number;
} __attribute__((packed));

extern "C" {
extern InterruptHandlerEntry interrupt_handler_table[];
extern void syscall_handler();
extern void load_cs_selector();
}

void VM::Load(virt_addr_t syscall_stack_top) {
  // Kernel code segment.
  AddGDTEntry(kKernelCodeSegmentIndex,
              SegmentDescriptor().set_required_privileges(kKernelPrivilege).set_type(SegmentDescriptor::kCodeSegment));

  // Kernl stack segment.
  AddGDTEntry(kKernelStackSegmentIndex,
              SegmentDescriptor().set_required_privileges(kKernelPrivilege).set_type(SegmentDescriptor::kDataSegment));

  // User code segment.
  AddGDTEntry(kUserCodeSegmentIndex,
              SegmentDescriptor().set_required_privileges(kUserPrivilege).set_type(SegmentDescriptor::kCodeSegment));

  // User stack segment.
  AddGDTEntry(kUserStackSegmentIndex,
              SegmentDescriptor().set_required_privileges(kUserPrivilege).set_type(SegmentDescriptor::kDataSegment));

  // TSS.
  TaskStateSegment tss;
  tss.set_privileged_stack(0, syscall_stack_top);
  tss.set_interrupt_stack(1, syscall_stack_top);
  tss.Serialize(tss_);

  SegmentDescriptor tss_desc;
  tss_desc.set_type(SegmentDescriptor::kTaskStateSegment);
  tss_desc.set_base(virt_addr_t(&tss_));
  tss_desc.set_size(sizeof(tss_));
  AddGDTEntry(kTSSIndex, tss_desc);

  env_->LoadGDT(virt_addr_t(&gdt_), kNumGDTEntries * sizeof(SegmentDescriptor::Storage));

  env_->LoadTSS(SegmentSelector(kTSSIndex));

  g_serial->Printf("Handler table = %p\n", interrupt_handler_table);

  for (int i = 0; interrupt_handler_table[i].handler != 0; i++) {
    const InterruptHandlerEntry& entry = interrupt_handler_table[i];
    g_serial->Printf("Handler %d = %d/%p\n", i, entry.number, entry.handler);
    AddIDTEntry(entry.number, InterruptDescriptor().set_offset(entry.handler).set_interrupt_stack(1));
  }

  // The system call handler.
  AddIDTEntry(0x80, InterruptDescriptor().set_offset(reinterpret_cast<virt_addr_t>(syscall_handler)).set_interrupt_stack(1));

  env_->LoadIDT(virt_addr_t(&idt_), kNumIDTEntries * sizeof(InterruptDescriptor::Storage));
}
