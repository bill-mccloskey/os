#include "protection.h"
#include "serial.h"
#include "string.h"

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
  bool is_code_or_data : 1;
  uint8_t descriptor_privilege_level : 2;
  bool present : 1;

  uint8_t limit_high : 4;
  bool reserved : 1;
  bool segment_long : 1;
  bool op_size_32_bits : 1;
  bool page_granularity : 1;

  uint8_t base_high;
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

} // namespace

void SegmentSelector::Serialize(Storage storage) const {
  SegmentSelectorLayout d = {};

  d.requested_privilege_level = requested_priv_;
  d.is_ldt = is_ldt_;
  d.index = selector_;

  memcpy(storage, &d, sizeof(d));
}

void SegmentDescriptor::Serialize(Storage storage) const {
  const int kBaseMiddleShift = 16;
  const int kBaseHighShift = 24;

  const int kByteGranularityMax = 0xfffff;
  const int kLimitHighShift = 16;
  const int kPageGranularityShift = 12;

  SegmentDescriptorLayout d = {};

  d.base_low = uint16_t(base_);
  d.base_middle = uint8_t(base_ >> kBaseMiddleShift);
  d.base_high = uint8_t(base_ >> kBaseHighShift);

  d.is_code_or_data = true;
  d.descriptor_privilege_level = required_priv_;
  d.present = true;
  d.segment_long = segment_long_;
  d.op_size_32_bits = !segment_long_;
  d.is_code_or_data = is_code_or_data_;

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

  if (type_ == kCodeSegment) {
    d.type = 0b1010;
  } else if (type_ == kDataSegment) {
    d.type = 0b0011;
  }

  memcpy(storage, &d, sizeof(d));
}

void InterruptDescriptor::Serialize(Storage storage) const {
  InterruptDescriptorLayout d = {};

  d.offset_low = uint16_t(offset_);
  segment_.Serialize(d.segment_selector);
  d.interrupt_stack = interrupt_stack_;
  d.is_trap = !disable_interrupts_;
  d.must_be_one = 0x7;
  d.descriptor_privilege_level = required_priv_;
  d.present = true;
  d.offset_middle = uint16_t(offset_ >> 16);
  d.offset_high = uint32_t(offset_ >> 32);

  memcpy(storage, &d, sizeof(d));
}

void VMEnv::LoadGDT(phys_addr_t base, size_t size) {
  struct Descriptor {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed));

  Descriptor desc __attribute__((aligned(8)));

  desc.base = base;
  desc.limit = size - 1;

  asm("lgdt (%0)" : : "r"(&desc));
}

void VMEnv::LoadIDT(phys_addr_t base, size_t size) {
  struct Descriptor {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed));

  Descriptor desc __attribute__((aligned(8)));

  desc.base = base;
  desc.limit = size - 1;

  asm("lidt (%0)" : : "r"(&desc));
}

VM::VM(VMEnv* env) : env_(env) {}

void VM::AddGDTEntry(const SegmentDescriptor& segdesc) {
  segdesc.Serialize(gdt_[num_gdt_entries_++]);
}

void VM::AddIDTEntry(int number, const InterruptDescriptor& desc) {
  desc.Serialize(idt_[number]);
}

struct InterruptHandlerEntry {
  phys_addr_t handler;
  int number;
} __attribute__((packed));

extern "C" {
extern InterruptHandlerEntry interrupt_handler_table[];
extern void load_cs_selector();
}

void VM::Load() {
  AddGDTEntry(SegmentDescriptor().set_required_privileges(kKernelPrivilege).set_type(SegmentDescriptor::kCodeSegment));
  AddGDTEntry(SegmentDescriptor().set_required_privileges(kUserPrivilege).set_type(SegmentDescriptor::kCodeSegment));
  AddGDTEntry(SegmentDescriptor().set_required_privileges(kUserPrivilege).set_type(SegmentDescriptor::kDataSegment));
  env_->LoadGDT(phys_addr_t(&gdt_), num_gdt_entries_ * sizeof(SegmentDescriptor::Storage));

  serial_printf(SERIAL_COM1_BASE, "Handler table = %p\n", interrupt_handler_table);

  for (int i = 0; interrupt_handler_table[i].handler != 0; i++) {
    const InterruptHandlerEntry& entry = interrupt_handler_table[i];
    serial_printf(SERIAL_COM1_BASE, "Handler %d = %d/%p\n", i, entry.number, entry.handler);
    AddIDTEntry(entry.number, InterruptDescriptor().set_offset(entry.handler));
  }
  env_->LoadIDT(phys_addr_t(&idt_), kNumIDTEntries * sizeof(InterruptDescriptor::Storage));
}
