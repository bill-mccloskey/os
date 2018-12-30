section .multiboot_header
header_start:
  dd 0xe85250d6                ; magic number (multiboot 2)
  dd 0                         ; architecture 0 (protected mode i386)
  dd header_end - header_start ; header length
  ; checksum
  dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

  ; insert optional multiboot tags here

  dw 5                          ; Framebuffer tag
  dw 0                          ; Flags
  dd 20                         ; Size
  dd 800                        ; Width
  dd 600                        ; Height
  dd 32                         ; Bits per pixel
  dd 0                          ; Padding

  dw 6                          ; Module alignment tag
  dw 0                          ; Flags
  dd 8                          ; Size

  ; required end tag
  dw 0    ; type
  dw 0    ; flags
  dd 8    ; size
header_end:
