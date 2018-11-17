#ifndef protection_h
#define protection_h

#include "types.h"

const int kKernelPrivilege = 0;
const int kUserPrivilege = 3;

const int kCodeSegmentIndex = 1;

class SegmentSelector {
public:
  SegmentSelector(uint16_t selector,
                  int requested_priv = kKernelPrivilege,
                  bool is_ldt = false)
    : requested_priv_(requested_priv),
      is_ldt_(is_ldt),
      selector_(selector) {}

  using Storage = uint8_t[2];

  void Serialize(Storage storage) const;

private:
  int requested_priv_ = kKernelPrivilege;
  bool is_ldt_ = false;
  uint16_t selector_ = 0;
};

class SegmentDescriptor {
public:
  enum Type {
    kCodeSegment,
    kDataSegment,
  };

  SegmentDescriptor& set_segment_long(bool is_long) { segment_long_ = is_long; return *this; }
  SegmentDescriptor& set_required_privileges(int priv) { required_priv_ = priv; return *this; }
  SegmentDescriptor& set_present(bool present) { present_ = present; return *this; }

  SegmentDescriptor& set_base(phys_addr_t base) { base_ = base; return *this; }
  SegmentDescriptor& set_size(size_t size) { last_byte_index_ = size - 1; return *this; }
  SegmentDescriptor& set_system_segment() { is_code_or_data_ = false; return *this; }
  SegmentDescriptor& set_type(Type type) { type_ = type; return *this; }

  using Storage = uint8_t[8];

  void Serialize(Storage storage) const;

private:
  phys_addr_t base_ = 0;
  size_t last_byte_index_ = 0xffffffff;
  int required_priv_ = kUserPrivilege;
  bool is_code_or_data_ = true; // false means a system segment
  bool present_ = true;
  Type type_ = kDataSegment;
  bool segment_long_ = true;
};

class InterruptDescriptor {
public:
  InterruptDescriptor() :
    segment_(kCodeSegmentIndex) {}

  InterruptDescriptor& set_offset(phys_addr_t offset) { offset_ = offset; return *this; }
  InterruptDescriptor& set_required_privileges(int priv) { required_priv_ = priv; return *this; }
  InterruptDescriptor& set_segment_selector(const SegmentSelector& selector) { segment_ = selector; return *this; }
  InterruptDescriptor& set_interrupt_stack(int stack) { interrupt_stack_ = stack; return *this; }
  InterruptDescriptor& set_disable_interrupts(bool disable) { disable_interrupts_ = disable; return *this; }

  using Storage = uint8_t[16];

  void Serialize(Storage storage) const;

private:
  phys_addr_t offset_;
  SegmentSelector segment_;
  int required_priv_ = kUserPrivilege;
  int interrupt_stack_ = 0;
  bool disable_interrupts_ = true;
};

class VMEnv {
public:
  virtual void LoadGDT(phys_addr_t base, size_t size);
  virtual void LoadIDT(phys_addr_t base, size_t size);
};

class VM {
public:
  VM(VMEnv* env);

  void Load();

private:
  void AddGDTEntry(const SegmentDescriptor& segdesc);
  void AddIDTEntry(int number, const InterruptDescriptor& desc);

  VMEnv* const env_;

  static const int kNumGDTEntries = 32;
  SegmentDescriptor::Storage gdt_[kNumGDTEntries] __attribute__((aligned(8))) = {};
  int num_gdt_entries_ = 1;

  static const int kNumIDTEntries = 64;
  InterruptDescriptor::Storage idt_[kNumIDTEntries] __attribute__((aligned(8))) = {};
};

#endif  // protection_h
