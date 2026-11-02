OUTPUT_ARCH(m68k)
ENTRY(cd_worker_start)
PHDRS { prg PT_LOAD FLAGS(7); }
SECTIONS {
    .vectors 0 : { *(.vectors) } :prg
    .text 0x200 : { *(.text .text.*) *(.ramtext) *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
}
ASSERT(. < 0x10000, "CD worker exceeds first PRG-RAM bank")
