/* Adapted from andwn/marsdev 32x-skeleton; see LICENSE.marsdev. */
OUTPUT_ARCH(sh)
SEARCH_DIR(.)

/* The memory map look like this:
 * +--------------------+ <- 0x02000000
 * | .text              |
 * |        _etext      |
 * |        ctor list   | the ctor and dtor lists are for
 * |        dtor list   | C++ support
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * +--------------------+ <- 0x06000000
 * | .data              | initialized data goes here
 * |        _edata      |
 * +--------------------+
 * | .bss               |
 * |        __bss_start | start of bss, cleared by crt0
 * |        _end        | start of heap, used by sbrk()
 * +--------------------+
 * .                    .
 * .                    .
 * .                    .
 * |        __stack     | top of stack (for Master SH2)
 * +--------------------+ <- 0x0603FC00
 */

MEMORY {
	rom (rx) : ORIGIN = 0x02000000, LENGTH = 0x00400000
	ram (wx) : ORIGIN = 0x06000000, LENGTH = 0x0003FC00
}

PROVIDE (__stack = 0x0603FC00);
/* SH2 assembly and the audio IRQ execute from writable SDRAM. */
PHDRS { rom PT_LOAD FLAGS(5); ram PT_LOAD FLAGS(7); }

SECTIONS {
	.text 0x02000000 : AT(0x00000000) {
		*(.text .text.*)
		. = ALIGN(0x4);

		__CTOR_LIST__ = .;
		___CTOR_LIST__ = .;
		LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
		*(.ctors)
		LONG(0)
		__CTOR_END__ = .;
		__DTOR_LIST__ = .;
		___DTOR_LIST__ = .;
		LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
		*(.dtors)
		LONG(0)
		__DTOR_END__ = .;
		*(.rodata .rodata.*)
		*(.gcc_except_table .gcc_except_table.*)

		. = ALIGN(0x10);
		__INIT_SECTION__ = . ;
		*(.init)
		SHORT (0x000B)	/* rts */
		SHORT (0x0009)	/* nop */

		. = ALIGN(0x10);
		__FINI_SECTION__ = . ;
		*(.fini)
		SHORT (0x000B)	/* rts */
		SHORT (0x0009)	/* nop */

		_etext = .;
		*(.lit)
		. = ALIGN(0x4);
	} > rom :rom
	_stext = SIZEOF (.text);
 __text_end = 0x02000000 + SIZEOF(.text);

	.data 0x06000000 : AT(LOADADDR(.text) + SIZEOF(.text)) {
		*(.got.plt) *(.got)
		*(.shdata)
		*(.data .data.*)
		*(.ramtext .ramtext.*)
		_edata = .;
		. = ALIGN(0x4);
	} > ram :ram
	_sdata = SIZEOF (.data);
 __data_size = SIZEOF(.data);

	.bss 0x06000000 + SIZEOF(.data) (NOLOAD) :
	{
		__bss_start = . ;
		*(.shbss)
		*(EXCLUDE_FILE (*common.o) .bss .bss.*)
		*(COMMON)
        /* Half of (table + 256) shares an accumulator with shade bits 8..12.
         * Bias before alignment avoids allocating a mostly empty prefix.
         * Baseline/test builds without this table do not need the padding. */
        . = DEFINED(_gLightmapPairs) ? ALIGN(ABSOLUTE(.) + 256, 16384) - 256 : ABSOLUTE(.);
        *(.lightmap_pairs)
        /* Leave the large common state after the table, so small test/debug
         * additions do not push alignment to the next 16 KiB boundary. */
        *common.o(.bss .bss.*)
		__bss_end = ALIGN(4);
 _end = ALIGN(8);
		__end = _end;
		end = _end;
	} > ram :ram

	.eh_frame 0 (NOLOAD) :
	{
		*(.eh_frame)
	}
}

ASSERT(__bss_end <= 0x0603E800, "32X SDRAM overlaps reserved stacks")

PROVIDE(_gLightmapPairsHalf = (_gLightmapPairs + 256) / 2);
ASSERT(DEFINED(_gLightmapPairs) ? (((_gLightmapPairs + 256) & 16383) == 0) : 1, "GT lightmap base alignment")
