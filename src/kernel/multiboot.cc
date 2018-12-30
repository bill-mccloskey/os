#include "multiboot.h"
#include "types.h"

static const int kMultibootModuleTag = 3;
static const int kMultibootMemoryMapTag = 6;
static const int kMultibootFramebufferTag = 8;

struct MultibootTag {
  uint32_t type;
  uint32_t size;
} __attribute__((packed));

struct MultibootModuleTag {
  MultibootTag tag;
  uint32_t module_start;
  uint32_t module_end;
  char label[1];
} __attribute__((packed));

struct MultibootMemoryMapTag {
  MultibootTag tag;
  uint32_t entry_size;
  uint32_t entry_version;
  // entries follow
} __attribute__((packed));

struct MultibootMemoryMapEntry {
  uint64_t base_addr;
  uint64_t length;
  uint32_t type;
  uint32_t reserved;
} __attribute__((packed));

struct MultibootFrambufferRGBInfo {
  uint8_t framebuffer_red_field_position;
  uint8_t framebuffer_red_mask_size;
  uint8_t framebuffer_green_field_position;
  uint8_t framebuffer_green_mask_size;
  uint8_t framebuffer_blue_field_position;
  uint8_t framebuffer_blue_mask_size;
} __attribute__((packed));

union MultibootFramebufferColorInfo {
  MultibootFrambufferRGBInfo rgb;
};

struct MultibootFramebufferTag {
  MultibootTag tag;
  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;
  uint8_t reserved;
  MultibootFramebufferColorInfo color_info;
} __attribute__((packed));

struct MultibootHeader {
  uint32_t total_size;
  uint32_t reserved;
} __attribute__((packed));

void MultibootReader::Read(MultibootVisitor* visitor) const {
  const MultibootHeader* header = reinterpret_cast<const MultibootHeader*>(header_);

  const char* p = reinterpret_cast<const char*>(header + 1);
  const char* end = reinterpret_cast<const char*>(header) + header->total_size;

  while (p < end) {
    const MultibootTag* tag = reinterpret_cast<const MultibootTag*>(p);

    visitor->StartTag(tag->type);

    if (tag->type == kMultibootModuleTag) {
      const MultibootModuleTag* module = reinterpret_cast<const MultibootModuleTag*>(tag);
      visitor->Module(module->label, module->module_start, module->module_end);
    }

    if (tag->type == kMultibootMemoryMapTag) {
      const MultibootMemoryMapTag* mem = reinterpret_cast<const MultibootMemoryMapTag*>(tag);
      const char* entry_p = reinterpret_cast<const char*>(mem + 1);
      const char* entry_end = p + tag->size;
      visitor->StartMemoryMap();
      while (entry_p < entry_end) {
        const MultibootMemoryMapEntry* entry = reinterpret_cast<const MultibootMemoryMapEntry*>(entry_p);
        visitor->MemoryMapEntry(entry->base_addr, entry->length, static_cast<MemoryMapEntryType>(entry->type));
        entry_p += mem->entry_size;
      }
      visitor->EndMemoryMap();
    }

    if (tag->type == kMultibootFramebufferTag) {
      const MultibootFramebufferTag* fb = reinterpret_cast<const MultibootFramebufferTag*>(tag);
      visitor->Framebuffer(fb->framebuffer_addr, fb->framebuffer_pitch,
                           fb->framebuffer_width, fb->framebuffer_height,
                           fb->framebuffer_bpp);

      if (fb->framebuffer_type == 1) {
        // RGB color.
        const MultibootFrambufferRGBInfo& rgb = fb->color_info.rgb;
        visitor->FramebufferRGBInfo(rgb.framebuffer_red_field_position, rgb.framebuffer_red_mask_size,
                                    rgb.framebuffer_green_field_position, rgb.framebuffer_green_mask_size,
                                    rgb.framebuffer_blue_field_position, rgb.framebuffer_blue_mask_size);
      }
    }

    visitor->EndTag();

    // Next tag will always be 8-byte aligned.
    uint32_t size_with_padding = ((tag->size + 7) >> 3) << 3;
    p += size_with_padding;
  }
}

