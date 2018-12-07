; Syscall number will be in RAX.
; Arguments will be in RDI, RSI, RDX, RCX, R8, R9

section .text

%macro gen_syscall 2
global sys_%1
sys_%1:
  mov rax, %2
  int 0x80
  ret
%endmacro

gen_syscall write_byte, 1
gen_syscall reschedule, 2
gen_syscall exit_thread, 3
