/* Real-mode BIOS EDD trampoline for the standalone i386 C++ kernel.
 * One 512-byte sector per operation. No BIOS services in protected mode.
 * Low memory: boot DL at 0x0500, EDD DAP at 0x5000, buffer at 0x5200.
 * Calls serialized from shell; not safe from interrupts or re-entrancy.
 */
.section .text,"ax"
.code32
.global bios_sector
.type bios_sector,@function
bios_sector:
    pushal
    cli
    movl %esp, saved_esp
    sidt saved_idt
    movl 36(%esp), %eax     /* absolute LBA */
    movl %eax, 0x5008
    movl $0, 0x500c
    movb $16, 0x5000
    movb $0, 0x5001
    movw $1, 0x5002
    movw $0x5200, 0x5004
    movw $0, 0x5006
    movl 40(%esp), %eax     /* 0 read, 1 write */
    movb %al, 0x5010
    movb $1, 0x5011       /* failure default */
    lgdt bios_tr_gdtdesc
    lidt real_idt
    /* PM16 code descriptor: base=0x10000 and 16-bit offset <0x10000. */
    ljmp $0x18, $__bios_pm16_off
.code16
 .global bios_pm16
bios_pm16:
    movw $0x20, %ax       /* PM16 data descriptor with base=0 */
    movw %ax, %ds
    movw %ax, %es
    movl %cr0, %eax
    andl $0xfffffffe, %eax
    movl %eax, %cr0
    ljmp $0x1000, $__bios_real_off
.global bios_real_entry
bios_real_entry:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7000, %sp
    call pic_bios
    movw $0x5000, %si
    movb 0x0500, %dl
    cmpb $0, 0x5010
    je bios_read
    movb $0x43, %ah       /* EDD write, AL=0 */
    xorb %al, %al
    jmp bios_call
bios_read:
    movb $0x42, %ah
bios_call:
    sti
    int $0x13
    cli
    setc %bl
    movb %ah, %bh
    xorw %ax, %ax
    movw %ax, %ds
    movb %bl, 0x5011
    movb %bh, 0x5012
    call pic_kernel
    movw $0x1000, %ax
    movw %ax, %ds
    lgdt __bios_gdt_off
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $pm_resume
/* PIC initialization restores offsets, masks and IRQ cascade. */
pic_bios:
    movb $0x11, %al
    outb %al, $0x20
    outb %al, $0xa0
    movb $0x08, %al
    outb %al, $0x21
    movb $0x70, %al
    outb %al, $0xa1
    movb $4, %al
    outb %al, $0x21
    movb $2, %al
    outb %al, $0xa1
    movb $1, %al
    outb %al, $0x21
    outb %al, $0xa1
    movb $0xf8, %al      /* timer + keyboard + cascade */
    outb %al, $0x21
    movb $0, %al        /* allow BIOS USB/IDE IRQ handlers */
    outb %al, $0xa1
    ret
pic_kernel:
    movb $0x11, %al
    outb %al, $0x20
    outb %al, $0xa0
    movb $0x20, %al
    outb %al, $0x21
    movb $0x28, %al
    outb %al, $0xa1
    movb $4, %al
    outb %al, $0x21
    movb $2, %al
    outb %al, $0xa1
    movb $1, %al
    outb %al, $0x21
    outb %al, $0xa1
    movb $0xfe, %al
    outb %al, $0x21
    movb $0xff, %al
    outb %al, $0xa1
    ret
.code32
pm_resume:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl saved_esp, %esp
    lidt saved_idt
    popal
    movzbl 0x5011, %eax
    xorl $1, %eax
    sti
    ret
.balign 8
tr_gdt:
    .quad 0
    .quad 0x00cf9a000000ffff /* PM32 code */
    .quad 0x00cf92000000ffff /* PM32 data */
    /* PM16 code, base 0x10000, 64-KiB limit. */
    .quad 0x00009a010000ffff
    .quad 0x000092000000ffff /* PM16 data base 0 */
tr_gdtend:
.global bios_tr_gdtdesc
bios_tr_gdtdesc:
    .word tr_gdtend-tr_gdt-1
    .long tr_gdt
real_idt:
    .word 0x03ff
    .long 0
saved_idt:
    .space 6
saved_esp:
    .long 0
.section .note.GNU-stack,"",@progbits
