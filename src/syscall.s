global syscall

section .text
syscall:
  ; Syscall number will be in RDI.
  ; Arguments will be in RSI, RDX, RCX, R8, R9
  int 0x80
  ret

