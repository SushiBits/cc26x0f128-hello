/**
 * @file system_CC26x0F128.h
 * @author Max Chan <max@maxchan.info> with Copilot
 */

#ifndef SYSTEM_CC26x0F128_H
#define SYSTEM_CC26x0F128_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The device header SVDConv generates includes this file by name, so it has to
   sit next to it. The definitions belong to the platform layer. */

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CC26x0F128_H */
