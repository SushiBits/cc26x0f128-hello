/**
 * @file CC26x0F128_it.c
 * 
 * Startup code in C for Cortex-M microcontroller
 */

#include "CC26x0F128_it.h"

#include "CC26x0F128.h"
#include <string.h>

void __stack(void); // Stack pointer, to be palced by linker script.
void _start(void); // C runtime entry point.
void Reset_IRQHandler(void);
static void Default_IRQHandler(void);

__attribute__((used, section(".vector"))) vector_t vector[] = {
    [0] = __stack,
    [1] = Reset_IRQHandler,
#define IRQN_HANDLER(idx, name) [idx + 16] = name##_IRQHandler,
#define SKIP_HANDLER(idx) [idx + 16] = Default_IRQHandler,
#include "CC26x0F128_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER
};

#define IRQN_HANDLER(idx, name) __attribute__((weak, alias("Default_IRQHandler"))) void name##_IRQHandler(void);
#define SKIP_HANDLER(idx)
#include "CC26x0F128_handlers.h"
#undef IRQN_HANDLER
#undef SKIP_HANDLER

extern uint8_t __vector_start__;
extern uint8_t __vector_load__;
extern uint8_t __vector_end__;

extern uint8_t __data_start__;
extern uint8_t __data_load__;
extern uint8_t __data_end__;

extern uint8_t __bss_start__;
extern uint8_t __bss_end__;

void Reset_IRQHandler(void) {

    SystemInit(); // Initialize system clock

    memcpy(&__vector_start__, &__vector_load__, &__vector_end__ - &__vector_start__);
    memcpy(&__data_start__, &__data_load__, &__data_end__ - &__data_start__);
    memset(&__bss_start__, 0, &__bss_end__ - &__bss_start__);

    SCB->VTOR = (uint32_t)vector;

    SystemCoreClockUpdate();

    _start();

    for (;;) __WFE();
}

static void Default_IRQHandler(void) {
    __BKPT();
    for (;;) __WFE();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
__attribute__((weak, alias("Default_IRQHandler"))) void _exit(void);
#pragma GCC diagnostic pop
