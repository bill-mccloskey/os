extern kinterrupt
global interrupt_handler_table
global some_code

some_code:
  xchg bx, bx
.loop:
  jmp .loop

%macro handler_no_error 1
global int%1_handler
int%1_handler:
  push 0
  push %1
  jmp common_interrupt_handler
%endmacro

%macro handler_with_error 1
global int%1_handler
int%1_handler:
  push dword %1
  jmp common_interrupt_handler
%endmacro

common_interrupt_handler:
  ;xchg bx, bx
  push rax
  push rcx
  push rdx
  push rsi
  push rdi
  push r8
  push r9
  push r10
  push r11
  mov rdi, rsp
  add rdi, 72
  call kinterrupt
  pop r11
  pop r10
  pop r9
  pop r8
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop rax
  add rsp, 16
  iretq

handler_no_error 0
handler_no_error 1
handler_no_error 2
handler_no_error 3
handler_no_error 4
handler_no_error 5
handler_no_error 6
handler_no_error 7
handler_with_error 8
handler_no_error 9
handler_with_error 10
handler_with_error 11
handler_with_error 12
handler_with_error 13
handler_with_error 14
handler_no_error 15
handler_no_error 16
handler_with_error 17
handler_no_error 18
handler_no_error 19
handler_no_error 20

handler_no_error 32
handler_no_error 33
handler_no_error 34
handler_no_error 35
handler_no_error 36
handler_no_error 37
handler_no_error 38
handler_no_error 39
handler_no_error 40
handler_no_error 41
handler_no_error 42
handler_no_error 43
handler_no_error 44
handler_no_error 45
handler_no_error 46
handler_no_error 47

align 8
interrupt_handler_table:
  dq int0_handler
  dd 0
  dq int1_handler
  dd 1
  dq int2_handler
  dd 2
  dq int3_handler
  dd 3
  dq int4_handler
  dd 4
  dq int5_handler
  dd 5
  dq int6_handler
  dd 6
  dq int7_handler
  dd 7
  dq int8_handler
  dd 8
  dq int9_handler
  dd 9
  dq int10_handler
  dd 10
  dq int11_handler
  dd 11
  dq int12_handler
  dd 12
  dq int13_handler
  dd 13
  dq int14_handler
  dd 14
  dq int15_handler
  dd 15
  dq int16_handler
  dd 16
  dq int17_handler
  dd 17
  dq int18_handler
  dd 18
  dq int19_handler
  dd 19
  dq int20_handler
  dd 20
  dq int32_handler
  dd 32
  dq int33_handler
  dd 33
  dq int34_handler
  dd 34
  dq int35_handler
  dd 35
  dq int36_handler
  dd 36
  dq int37_handler
  dd 37
  dq int38_handler
  dd 38
  dq int40_handler
  dd 40
  dq int41_handler
  dd 41
  dq int42_handler
  dd 42
  dq int43_handler
  dd 43
  dq int44_handler
  dd 44
  dq int45_handler
  dd 45
  dq int46_handler
  dd 46
  dq int47_handler
  dd 47
  dq 0
  dd 0
