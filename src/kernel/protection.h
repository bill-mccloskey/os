#ifndef protection_h
#define protection_h

#include "base/types.h"
#include "kernel/frame_allocator.h"

const int kKernelPrivilege = 0;
const int kUserPrivilege = 3;

const int kKernelCodeSegmentIndex = 1;
const int kKernelStackSegmentIndex = 2;
const int kUserCodeSegmentIndex = 3;
const int kUserStackSegmentIndex = 4;
const int kTSSIndex = 5;

class SegmentSelector {
public:
  SegmentSelector(uint16_t selector,
                  int requested_priv = kKernelPrivilege,
                  bool is_ldt = false)
    : requested_priv_(requested_priv),
      is_ldt_(is_ldt),
      selector_(selector) {}

  using Storage = uint16_t;

  Storage Serialize() const;

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
    kTaskStateSegment,
  };

  SegmentDescriptor& set_segment_long(bool is_long) { segment_long_ = is_long; return *this; }
  SegmentDescriptor& set_required_privileges(int priv) { required_priv_ = priv; return *this; }
  SegmentDescriptor& set_present(bool present) { present_ = present; return *this; }

  SegmentDescriptor& set_base(virt_addr_t base) { base_ = base; return *this; }
  SegmentDescriptor& set_size(size_t size) { last_byte_index_ = size - 1; return *this; }
  SegmentDescriptor& set_type(Type type) { type_ = type; return *this; }

  using Storage = uint8_t[8];

  // Returns the number of slots used by the descriptor. (A system descriptor uses two slots.)
  int Serialize(uint8_t* dest) const;

private:
  virt_addr_t base_ = 0;
  size_t last_byte_index_ = 0xffffffff;
  int required_priv_ = kUserPrivilege;
  bool present_ = true;
  Type type_ = kDataSegment;
  bool segment_long_ = true;
};

class InterruptDescriptor {
public:
  InterruptDescriptor() :
    segment_(kKernelCodeSegmentIndex) {}

  InterruptDescriptor& set_offset(virt_addr_t offset) { offset_ = offset; return *this; }
  InterruptDescriptor& set_required_privileges(int priv) { required_priv_ = priv; return *this; }
  InterruptDescriptor& set_segment_selector(const SegmentSelector& selector) { segment_ = selector; return *this; }
  InterruptDescriptor& set_interrupt_stack(int stack) { interrupt_stack_ = stack; return *this; }
  InterruptDescriptor& set_disable_interrupts(bool disable) { disable_interrupts_ = disable; return *this; }

  using Storage = uint8_t[16];

  void Serialize(Storage storage) const;

private:
  virt_addr_t offset_;
  SegmentSelector segment_;
  int required_priv_ = kUserPrivilege;
  int interrupt_stack_ = 0;
  bool disable_interrupts_ = true;
};

class TaskStateSegment {
public:
  TaskStateSegment& set_privileged_stack(int index, virt_addr_t stack) {
    // FIXME: Assert index < kNumPrivilegedStacks
    privileged_stacks_[index] = stack;
    return *this;
  }
  TaskStateSegment& set_interrupt_stack(int index, virt_addr_t stack) {
    // FIXME: Assert index < kNumInterruptStacks
    interrupt_stacks_[index - 1] = stack;
    return *this;
  }

  using Storage = uint8_t[104];

  void Serialize(Storage storage) const;

private:
  static const int kNumPrivilegedStacks = 3;
  static const int kNumInterruptStacks = 7;

  virt_addr_t privileged_stacks_[3];
  virt_addr_t interrupt_stacks_[7];
};

class VMEnv {
public:
  virtual void LoadGDT(virt_addr_t base, size_t size);
  virtual void LoadIDT(virt_addr_t base, size_t size);
  virtual void LoadTSS(const SegmentSelector& selector);
};

class VM {
public:
  VM(VMEnv* env);

  void Load(virt_addr_t syscall_stack_top);

private:
  void AddGDTEntry(int number, const SegmentDescriptor& segdesc);
  void AddIDTEntry(int number, const InterruptDescriptor& desc);

  VMEnv* const env_;

  static const int kNumGDTEntries = 8;
  SegmentDescriptor::Storage gdt_[kNumGDTEntries] __attribute__((aligned(8))) = {};

  static const int kNumIDTEntries = 256;
  InterruptDescriptor::Storage idt_[kNumIDTEntries] __attribute__((aligned(8))) = {};

  TaskStateSegment::Storage tss_ __attribute__((aligned(8))) = {};
};

extern VM* g_vm;

#endif  // protection_h
