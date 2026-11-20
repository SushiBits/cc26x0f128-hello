/**
 * @file main.c
 */

#include "CC26x0F128.h"

#include "systick.h"

void SystemInit(void) {
    // Our board have both 24MHz and 32.768kHz XOSC's. We make use of both of them.
    
}

int main(void) {
    systick_init();

    for (;;) {
        delay(500);
    }
}
