#ifndef elf_h
#define elf_h

#include "base/types.h"

class ElfVisitor {
public:
  static const int kFlagExecute = 1;
  static const int kFlagWrite = 2;
  static const int kFlagRead = 4;

  virtual void LoadSegment(int flags, const char* data, size_t size, virt_addr_t load_addr, size_t load_size) {}
};

class ElfReader {
public:
  ElfReader(const char* data, size_t size);

  void Read(ElfVisitor* visitor);

  virt_addr_t entry_point() const { return entry_point_; }

private:
  const char* data_;
  size_t size_;
  virt_addr_t entry_point_ = 0;
};

#endif
