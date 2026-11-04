.section .start,"ax"
.code32
.global _start
.extern kernel_main
.extern __bss_start
.extern __bss_end
_start:
    cld
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    xorl %eax, %eax
    rep stosb
    xorl %ebp, %ebp
    call kernel_main
1:  cli
    hlt
    jmp 1b

.section .text
.global timer_stub
timer_stub:
    pushal
    cld
    call timer_interrupt
    popal
    iret
.global fault_stub
fault_stub:
    cli
    cld
    pushal
    call fatal_interrupt
2:  hlt
    jmp 2b
.section .note.GNU-stack,"",@progbits
