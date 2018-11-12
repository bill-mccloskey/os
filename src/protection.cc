#include "protection.h"
#include "string.h"

namespace {

struct SegmentSelectorLayout {
  uint8_t requested_privilege_level : 2;
  bool is_ldt : 1;
  uint16_t index : 13;
} __attribute__((packed));

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
  bool long_mode : 1;
  bool op_size_32_bits : 1;
  bool page_granularity : 1;

  uint8_t base_high;
} __attribute__((packed));

struct TrapDescriptorLayout {
  uint16_t offset_low;
  uint16_t segment_selector;

  uint8_t reserved : 1;
  uint8_t type : 4;
  uint8_t descriptor_privilege_level : 2;
  bool present : 1;

  uint16_t offset_high;
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
  d.long_mode = false;
  d.op_size_32_bits = true;
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

void VMEnv::LoadGDT(phys_addr_t base, size_t size) {
  struct Descriptor {
    uint16_t limit;
    uint32_t base;
  } __attribute__((packed));

  Descriptor desc __attribute__((aligned(8)));

  desc.base = base;
  desc.limit = size - 1;

  //asm("lgdtl (%0)" : : "r"(&desc));
  (void)desc;
}

void VMEnv::LoadSegmentSelectors(int cs_index, int ds_index) {
  if (cs_index == 1 && ds_index == 2) {
#if 0
    asm("movw $16, %ax\n"
        "movw %ax, %ds\n"
        "movw %ax, %es\n"
        "movw %ax, %fs\n"
        "movw %ax, %gs\n"
        "ljmp $8, $next\n"
        "next:\n");
#endif
  } else {
    //LOG(FATAL) << "Invalid segment values" << cs_index << " " << ds_index;
  }
}

VM::VM(VMEnv* env) : env_(env) {}

void VM::AddEntry(const SegmentDescriptor& segdesc) {
  segdesc.Serialize(gdt_[num_entries_++]);
}

void VM::Load() {
  // Add code segment
  AddEntry(SegmentDescriptor().set_required_privileges(0).set_type(SegmentDescriptor::kCodeSegment));

  // Add data segment
  AddEntry(SegmentDescriptor().set_required_privileges(0).set_type(SegmentDescriptor::kDataSegment));

  env_->LoadGDT(phys_addr_t(&gdt_), num_entries_ * sizeof(SegmentDescriptor::Storage));
  env_->LoadSegmentSelectors(1, 2);
}
