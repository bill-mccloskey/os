; Syscall number will be in RAX.
; Arguments will be in RDI, RSI, RDX, RCX, R8, R9

section .text

%macro gen_syscall 2
global Sys%1
Sys%1:
  mov rax, %2
  int 0x80
  ret
%endmacro

gen_syscall WriteByte, 1
gen_syscall Reschedule, 2
gen_syscall ExitThread, 3
gen_syscall Send, 4
gen_syscall Receive, 5
gen_syscall Notify, 6
gen_syscall RequestInterrupt, 7
gen_syscall AckInterrupt, 8
