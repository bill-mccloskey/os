extern kinterrupt
extern syscall_handler_table

global interrupt_handler_table
global syscall_handler
global SchedulerStart
global SwitchAddressSpace

; Warning: Any changes to this structure must be reflected in thread.h:ThreadState.
struc thread_state
ts_rip: resb 8
ts_cs: resb 8
ts_rflags: resb 8
ts_rsp: resb 8
ts_ss: resb 8
ts_rax: resb 8
ts_rbx: resb 8
ts_rcx: resb 8
ts_rdx: resb 8
ts_rsi: resb 8
ts_rdi: resb 8
ts_r8: resb 8
ts_r9: resb 8
ts_r10: resb 8
ts_r11: resb 8
ts_r12: resb 8
ts_r13: resb 8
ts_r14: resb 8
ts_r15: resb 8
ts_rbp: resb 8
endstruc

struc cpu_state
cpu_current_thread: resb 8
cpu_previous_thread: resb 8
endstruc

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
; We restore rdi from the ThreadState so we can pass an argument to
; thread startup.
SchedulerStart:
  push qword[rdi + ts_ss]
  push qword[rdi + ts_rsp]
  push qword[rdi + ts_rflags]
  push qword[rdi + ts_cs]
  push qword[rdi + ts_rip]
  mov rdi, qword[rdi + ts_rdi]
  iretq

SwitchAddressSpace:
  mov cr3, rdi
  ret

%macro restore_thread_regs 0
  ; Layout of the stack at this time:
  ; INTERRUPT_STACK_HIGH:
  ;   CpuState::previous_thread [rsp + 8]
  ;   CpuState::current_thread [rsp + 0]

  mov rax, qword[rsp]           ; Copy CpuState::current_thread to rax
  mov rbx, qword[rax + ts_rbx]
  mov rcx, qword[rax + ts_rcx]
  mov rdx, qword[rax + ts_rdx]
  mov rsi, qword[rax + ts_rsi]
  mov rdi, qword[rax + ts_rdi]
  mov r8, qword[rax + ts_r8]
  mov r9, qword[rax + ts_r9]
  mov r10, qword[rax + ts_r10]
  mov r11, qword[rax + ts_r11]
  mov r12, qword[rax + ts_r12]
  mov r13, qword[rax + ts_r13]
  mov r14, qword[rax + ts_r14]
  mov r15, qword[rax + ts_r15]
  mov rbp, qword[rax + ts_rbp]
  push qword[rax + ts_ss]
  push qword[rax + ts_rsp]
  push qword[rax + ts_rflags]
  push qword[rax + ts_cs]
  push qword[rax + ts_rip]
  mov rax, qword[rax + ts_rax]

  ; At this point, we should be in a state where iretq will finish
  ; restoring the state.
%endmacro

syscall_handler:
  ; AMD64 ABI:
  ; rbx, rbp, and r12-r15 are callee-saved registers.
  ; Others are caller-saved.
  ; Arguments passed in: rdi, rsi, rdx, rcx, r8, r9.
  ; rax is the return value.

  ; Layout of the stack at this time:
  ; INTERRUPT_STACK_HIGH: (the top of the page allocated for interrupts)
  ;   CpuState::previous_thread [rsp + 48]
  ;   CpuState::current_thread [rsp + 40]
  ;   ss [rsp + 32]
  ;   rsp [rsp + 24]
  ;   rflags [rsp + 16]
  ;   cs [rsp + 8]
  ;   rip [rsp + 0]

  mov r10, qword[rsp + 40]      ; Store CpuState::current_thread in r10
  pop qword[r10 + ts_rip]
  pop qword[r10 + ts_cs]
  pop qword[r10 + ts_rflags]
  pop qword[r10 + ts_rsp]
  pop qword[r10 + ts_ss]

  ; Callee-saved, so we need to save them.
  mov qword[r10 + ts_rbx], rbx
  mov qword[r10 + ts_r12], r12
  mov qword[r10 + ts_r13], r13
  mov qword[r10 + ts_r14], r14
  mov qword[r10 + ts_r15], r15
  mov qword[r10 + ts_rbp], rbp

  mov r10, syscall_handler_table
  mov r11, qword[r10 + rax * 8]
  call r11

  restore_thread_regs

  iretq

common_interrupt_handler:
  ; Bochs debugging instruction.
  ;xchg bx, bx

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
  mov qword[rax + ts_rbx], rbx
  mov qword[rax + ts_rcx], rcx
  mov qword[rax + ts_rdx], rdx
  mov qword[rax + ts_r8], r8
  mov qword[rax + ts_r9], r9
  mov qword[rax + ts_r10], r10
  mov qword[rax + ts_r11], r11
  mov qword[rax + ts_r12], r12
  mov qword[rax + ts_r13], r13
  mov qword[rax + ts_r14], r14
  mov qword[rax + ts_r15], r15
  mov qword[rax + ts_rbp], rbp

  ; Call kinterrupt(interrupt_number, error_code)
  call kinterrupt
  ; kinterrupt returns nothing in rax.

  restore_thread_regs

  ;xchg bx, bx

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
