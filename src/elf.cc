#include "elf.h"

#include <string.h>
#include <stdint.h>

#include "assertions.h"

typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef uint64_t Elf64_Off;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;

static const int EI_NIDENT = 16;
static const int ET_EXEC = 2;
static const int EM_X86_64 = 62;
static const int PT_LOAD = 1;

static const int PF_X = 1;
static const int PF_W = 2;
static const int PF_R = 4;

struct Elf64_Ehdr {
  unsigned char e_ident[EI_NIDENT];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr {
  Elf64_Word p_type;
  Elf64_Word p_flags;
  Elf64_Off p_offset;
  Elf64_Addr p_vaddr;
  Elf64_Addr p_paddr;
  Elf64_Xword p_filesz;
  Elf64_Xword p_memsz;
  Elf64_Xword p_align;
} __attribute__((packed));

static const char kExpectedElfIdent[EI_NIDENT] = {
  0x7f, 'E', 'L', 'F', // magic number
  2, // EI_CLASS = ELFCLASS64
  1, // EI_DATA = ELFDATA2LSB
  1, // EI_VERSION = EV_CURRENT
  0, // EI_OSABI = ELFOSABI_NONE
  // EI_PAD...
};

ElfReader::ElfReader(const char* data, size_t size)
  : data_(data), size_(size) {
  assert_ge(size_, sizeof(Elf64_Ehdr));
}

void ElfReader::Read(ElfVisitor* visitor) {
  const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>(data_);
  assert(memcmp(header->e_ident, kExpectedElfIdent, EI_NIDENT) == 0);

  assert_eq(header->e_type, ET_EXEC);
  assert_eq(header->e_machine, EM_X86_64);

  entry_point_ = header->e_entry;

  assert_ne(header->e_phoff, 0);
  assert_le(header->e_phoff + header->e_phnum * header->e_phentsize, size_);

  for (int i = 0; i < header->e_phnum; i++) {
    long offset = header->e_phoff + i * header->e_phentsize;
    const Elf64_Phdr* phdr = reinterpret_cast<const Elf64_Phdr*>(data_ + offset);

    if (phdr->p_type != PT_LOAD) continue;

    const char* p = data_ + phdr->p_offset;
    visitor->LoadSegment(phdr->p_flags, p, phdr->p_filesz, phdr->p_vaddr, phdr->p_memsz);
  }
}
