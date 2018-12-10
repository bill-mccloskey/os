global long_mode_start
extern kmain

section .early_text
bits 64
long_mode_start:
  ; load 0 into all data segment registers
  mov ax, 0
  mov ss, ax
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  ; relocate the stack pointer and the multiboot header to highmem
  mov rax, 0xffff800000000000
  add rsp, rax
  add rdi, rax

  ; call the rust main
  mov rax, kmain
  call rax

  ; print `OKAY` to screen
  mov rax, 0x2f592f412f4b2f4f
  mov qword [0xb8000], rax
  hlt
