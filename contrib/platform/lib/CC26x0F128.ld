/**
 * @file CC26x0F128.ld
 *
 * Memory layout for CC26x0F128, included by the board linker script.
 */

MEMORY
{
	IROM (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00007FF8
	IRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00001000
}
