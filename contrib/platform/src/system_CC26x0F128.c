/**
 * @file system_MSPM0L130X.c
 *
 * System clock accounting for the MSPM0L130x family.
 *
 * The TI SDK has no system file for the M0P devices: SystemInit and the
 * SystemCoreClock value are supplied by the application or by SysConfig. This
 * is that layer, kept separate from the startup code in MSPM0L130X_it.c so the
 * latter stays family-independent.
 */

#include "CC26x0F128.h"
#include "system_CC26x0F128.h"

uint32_t SystemCoreClock;

__attribute__((weak)) void SystemInit() {}
void SystemCoreClockUpdate(void) {}
