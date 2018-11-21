#ifndef multiboot_h
#define multiboot_h

#include "types.h"

enum class MemoryMapEntryType : uint32_t {
  kAvailableRAM = 1,
  kReserved = 2,
  kACPIReclaimable = 3,
};

class MultibootVisitor {
public:
  virtual void StartTag(int type) {}
  virtual void EndTag() {}

  virtual void Module(const char* label, uint32_t module_start, uint32_t module_end) {}

  virtual void StartMemoryMap() {}
  virtual void MemoryMapEntry(uint64_t base_addr, uint64_t length, MemoryMapEntryType type) {}
  virtual void EndMemoryMap() {}
};

class MultibootReader {
public:
  MultibootReader(const char* header) : header_(header) {}

  void Read(MultibootVisitor* visitor);

private:
  const char* header_;
};

#endif // multiboot_h
