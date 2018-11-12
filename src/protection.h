#ifndef protection_h
#define protection_h

#include "types.h"

const int kKernelPrivilege = 0;
const int kUserPrivilege = 3;

class SegmentSelector {
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

  SegmentDescriptor& set_base(phys_addr_t base) { base_ = base; return *this; }
  SegmentDescriptor& set_size(size_t size) { last_byte_index_ = size - 1; return *this; }
  SegmentDescriptor& set_required_privileges(int priv) { required_priv_ = priv; return *this; }
  SegmentDescriptor& set_system_segment() { is_code_or_data_ = false; return *this; }
  SegmentDescriptor& set_present(bool present) { present_ = present; return *this; }
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
};

class InterruptDescriptor {
  InterruptDescriptor& set_offset(phys_addr_t offset) { offset_ = offset; return *this; }
  InterruptDescriptor& set_required_privileges(int priv) { required_priv_ = priv; return *this; }
  InterruptDescriptor& set_segment_selector(const SegmentSelector& selector) { segment_ = selector; return *this; }

private:
  phys_addr_t offset_;
  SegmentSelector segment_;
  int required_priv_ = kUserPrivilege;
  bool present_ = true;
};

class VMEnv {
public:
  virtual void LoadGDT(phys_addr_t base, size_t size);
  virtual void LoadSegmentSelectors(int cs_index, int ds_index);
};

class VM {
public:
  VM(VMEnv* env);

  void Load();

private:
  void AddEntry(const SegmentDescriptor& segdesc);

  static const int kNumEntries = 32;

  VMEnv* const env_;
  SegmentDescriptor::Storage gdt_[kNumEntries] __attribute__((aligned(8))) = {};
  int num_entries_ = 1;
};

#endif  // protection_h
