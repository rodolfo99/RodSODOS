/* BIOS floppy boot sector. GNU as AT&T syntax; linked at 0000:7c00.
 * The builder supplies KERNEL_SECTORS. Each transfer is one sector so it
 * never crosses a track or a 64 KiB DMA boundary. No disk writes occur. */
.section .text
.code16
.global _start
_start:
    ljmp $0, $normalized
normalized:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    cld
    movb %dl, drive
    sti
    movw $0x0003, %ax
    int $0x10
    movw $0x1000, %ax
    movw %ax, %es
    movw $1, lba
    movw $KERNEL_SECTORS, remaining
read_next:
    movb $3, retries
retry:
    movw lba, %ax
    xorw %dx, %dx
    movw $18, %cx
    divw %cx
    incb %dl
    movb %dl, sector
    xorw %dx, %dx
    movw $2, %cx
    divw %cx
    movb %dl, %dh
    movb %al, %ch
    movb sector, %cl
    movb drive, %dl
    xorw %bx, %bx
    movw $0x0201, %ax
    pushw %es
    int $0x13
    popw %es
    jnc loaded
    xorw %ax, %ax
    movb drive, %dl
    pushw %es
    int $0x13
    popw %es
    decb retries
    jnz retry
    movw $error_text, %si
print_error:
    lodsb
    testb %al, %al
    jz halt
    movb $0x0e, %ah
    movw $7, %bx
    int $0x10
    jmp print_error
halt:
    cli
    hlt
    jmp halt
loaded:
    movw %es, %ax
    addw $0x20, %ax
    movw %ax, %es
    incw lba
    decw remaining
    jnz read_next
    cli
    // Fast A20 gate; the kernel currently fits below 1 MiB.
    inb $0x92, %al
    orb $2, %al
    andb $0xfe, %al
    outb %al, $0x92
    lgdt gdt_descriptor
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmpl $0x08, $protected_mode
.code32
protected_mode:
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
.balign 8
gdt:
    .quad 0
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:
gdt_descriptor:
    .word gdt_end-gdt-1
    .long gdt
drive: .byte 0
sector: .byte 0
retries: .byte 0
lba: .word 0
remaining: .word 0
error_text: .asciz "RodOS: error leyendo disquete."
.org 510
.word 0xaa55
