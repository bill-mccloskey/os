extern kinterrupt
global interrupt_handler_table
global scheduler_start

struc thread_state
ts_rip: resb 8
ts_cs: resb 8
ts_rflags: resb 8
ts_rsp: resb 8
ts_ss: resb 8
ts_rax: resb 8
ts_rcx: resb 8
ts_rdx: resb 8
ts_rsi: resb 8
ts_rdi: resb 8
ts_r8: resb 8
ts_r9: resb 8
ts_r10: resb 8
ts_r11: resb 8
endstruc

struc cpu_state
cpu_current_thread: resb 8
cpu_previous_thread: resb 8
endstruc

some_code:
  xchg bx, bx
  int 0x80
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

; This code is called at boot time to start scheduling.
; On function entry, rdi contains the ThreadState for the thread to start.
scheduler_start:
  push qword[rdi + ts_ss]
  push qword[rdi + ts_rsp]
  push qword[rdi + ts_rflags]
  push qword[rdi + ts_cs]
  push qword[rdi + ts_rip]
  iretq

common_interrupt_handler:
  ; Bochs debugging instruction.
  xchg bx, bx

  ; Push rax so we have a free register to work with.
  push rax

  ; Layout of the stack at this time:
  ; INTERRUPT_STACK_HIGH: (the top of the page allocated for interrupts)
  ;   CpuState::previous_thread [rsp + 72]
  ;   CpuState::current_thread [rsp + 64]
  ;   ss [rsp + 56]
  ;   rsp [rsp + 48]
  ;   rflags [rsp + 40]
  ;   cs [rsp + 32]
  ;   rip [rsp + 24]
  ;   error_code [rsp + 16]
  ;   interrupt_number [rsp + 8]
  ;   saved rax [rsp + 0]

  mov rax, qword[rsp + 64]      ; Store CpuState::current_thread in rax
  pop qword[rax + ts_rax]       ; Copy the saved rax value to the ThreadState
  mov qword[rax + ts_rdi], rdi  ; Save rdi and rsi to the ThreadState. These will be used for kinterrupt parameters.
  mov qword[rax + ts_rsi], rsi
  pop rdi                       ; Copy the interrupt number to rdi, the first kinterrupt parameter.
  pop rsi                       ; Copy the error code to rsi, second kinterrupt parameter.
  pop qword[rax + ts_rip]
  pop qword[rax + ts_cs]
  pop qword[rax + ts_rflags]
  pop qword[rax + ts_rsp]
  pop qword[rax + ts_ss]
  mov qword[rax + ts_rcx], rcx
  mov qword[rax + ts_rdx], rdx
  mov qword[rax + ts_r8], r8
  mov qword[rax + ts_r9], r9
  mov qword[rax + ts_r10], r10
  mov qword[rax + ts_r11], r11

  ; Call kinterrupt(interrupt_number, error_code)
  call kinterrupt
  ; kinterrupt returns nothing in rax.

  ; Layout of the stack at this time:
  ; INTERRUPT_STACK_HIGH:
  ;   CpuState::previous_thread [rsp + 8]
  ;   CpuState::current_thread [rsp + 0]

  mov rax, qword[rsp]           ; Copy CpuState::current_thread to rax
  mov rcx, qword[rax + ts_rcx]
  mov rdx, qword[rax + ts_rdx]
  mov rsi, qword[rax + ts_rsi]
  mov rdi, qword[rax + ts_rdi]
  mov r8, qword[rax + ts_r8]
  mov r9, qword[rax + ts_r9]
  mov r10, qword[rax + ts_r10]
  mov r11, qword[rax + ts_r11]
  push qword[rax + ts_ss]
  push qword[rax + ts_rsp]
  push qword[rax + ts_rflags]
  push qword[rax + ts_cs]
  push qword[rax + ts_rip]
  mov rax, qword[rax + ts_rax]
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

handler_no_error 128

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
  dq int128_handler
  dd 128
  dq 0
  dd 0
