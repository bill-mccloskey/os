#ifndef base_kernel_module_h
#define base_kernel_module_h

#include "types.h"

struct FrameBufferData {
  uint64_t addr;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint8_t bpp;
} __attribute__((packed));

struct KernelModuleData {
  FrameBufferData framebuffer;
} __attribute__((packed));

#endif
