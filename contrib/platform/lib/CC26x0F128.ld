/**
 * @file CC26x0F128.ld
 *
 * Memory layout for CC26x0F128, included by the board linker script.
 */

MEMORY
{
	IROM (rx)  : ORIGIN = 0x00000000, LENGTH = 0x0001FFA8
	CCFG (r)   : ORIGIN = 0x0001FFA8, LENGTH = 88
	IRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00005000
	GPRAM (rw) : ORIGIN = 0x11000000, LENGTH = 0x00002000
}
