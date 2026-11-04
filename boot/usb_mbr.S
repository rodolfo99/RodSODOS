/* RodOS USB MBR: BIOS x86 legacy/CSM + INT13 extensions.
 * Kernel is embedded in reserved space at LBA 256; FAT partition starts later.
 * Bootloader does not write to disk. Partition table occupies bytes 446-509. */
.section .text
.code16
.global _start
_start:
    ljmp $0, $begin
begin:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    cld
    movb %dl, 0x0500 /* BIOS boot disk for kernel */
    sti
    /* Read blocks via EDD. Keep DAP at low memory and destination in ES. */
    movw $0x1000, %ax
    movw %ax, target_segment
    movw $KERNEL_SECTORS, remaining
    movl $256, current_lba
read_sector:
    movb $3, attempts
retry:
    movw target_segment, %ax
    movw %ax, dap_segment
    movl current_lba, %eax
    movl %eax, dap_lba
    movw $dap, %si
    movb 0x0500, %dl
    movb $0x42, %ah
    int $0x13
    jnc okay
    xorw %ax, %ax
    movb 0x0500, %dl
    int $0x13
    decb attempts
    jnz retry
    movw $fail_msg, %si
print:
    lodsb
    testb %al, %al
    jz stop
    movb $0x0e, %ah
    movw $0x0007, %bx
    int $0x10
    jmp print
stop:
    cli
    hlt
    jmp stop
okay:
    addw $0x20, target_segment
    incl current_lba
    decw remaining
    jnz read_sector
    cli
    inb $0x92, %al
    orb $2, %al
    andb $0xfe, %al
    outb %al, $0x92
    lgdt gdtdesc
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $pmode
.code32
pmode:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x90000, %esp
    cld
    movl $0x10000, %eax
    jmp *%eax
.code16
.balign 8
gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdtend:
gdtdesc:
    .word gdtend-gdt-1
    .long gdt
/* DAP: 16 bytes, one sector, offset 0 and segment 0x1000 ... */
dap:
    .byte 16,0
    .word 1
    .word 0
dap_segment: .word 0x1000
dap_lba: .long 256
    .long 0
current_lba: .long 256
target_segment: .word 0x1000
remaining: .word 0
attempts: .byte 0
fail_msg: .asciz "RodOS: error BIOS USB (EDD)."
/* Partition table and 0x55AA installed by build_usb_iso.py. */
.org 446
