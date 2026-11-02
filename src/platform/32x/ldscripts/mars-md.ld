/* Adapted from andwn/marsdev 32x-skeleton; see LICENSE.marsdev. */
OUTPUT_ARCH(m68k)
SEARCH_DIR(.)

MEMORY {
	rom (rx) : ORIGIN = 0x00880000, LENGTH = 4M
	ram (wx) : ORIGIN = 0x00FF0000, LENGTH = 64K
}
ASSERT(__bss_end <= 0x00FFF000, "Genesis worker overlaps reserved stack")

PROVIDE (__stack = 0x01000000);
/* RAM contains executable worker code on this bare-metal target. */
PHDRS { rom PT_LOAD FLAGS(5); ram PT_LOAD FLAGS(7); }

SECTIONS {
	.text 0x00880800 : {
		__text_start = .;
		*(.text.keepboot)
		*(.text .text.*)
		*(.rodata .rodata.*)
		__text_end = ALIGN(2);
	} > rom :rom
	__text_size = SIZEOF(.text);

	.data 0x00FF0000 : AT(LOADADDR(.text) + SIZEOF(.text)) {
		__data_start = .;
		*(.data .data.*)
		*(.ramtext)
		__data_end = ALIGN(2);
	} > ram :ram
	__data_size = SIZEOF(.data);

	.bss 0x00FF0000 + SIZEOF(.data) : {
		__bss_start = . ;
		*(.bss .bss.*)
		__bss_end = ALIGN(2);
	} > ram :ram
	__bss_size = SIZEOF(.bss);
}
