OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0x10000;
    .text : { KEEP(*(.start)) *(.text*) } :text
    .rodata : ALIGN(16) { *(.rodata*) } :text
    .data : ALIGN(16) { *(.data*) } :data
    __bss_start = .;
    .bss (NOLOAD) : ALIGN(16) { *(.bss*) *(COMMON) } :data
    __bss_end = .;
    __bios_pm16_off = bios_pm16 - 0x10000;
    __bios_real_off = bios_real_entry - 0x10000;
    __bios_gdt_off = bios_tr_gdtdesc - 0x10000;
    ASSERT(__bios_pm16_off < 0x10000 && __bios_real_off < 0x10000, "BIOS trampoline above real-mode segment") 
    ASSERT(__bss_end < 0x70000, "Kernel overlaps reserved stack area")
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
