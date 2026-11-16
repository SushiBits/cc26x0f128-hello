/**
 * @file main.c
 */

#include "MSPM0L130X.h"

#include "systick.h"

void SystemInit(void) {}

// LED on PA6, which the SDK maps to pinCMx 7. The IOMUX_PINCMn constants are
// 0-based - IOMUX_PINCM1 is 0 and IOMUX_PINCM7 is 6 - so the array index is one
// less than the pinCM number.
#define LED_PINCM   6u
#define LED_PIN     (1u << 6)

// The GPRCM registers are write-keyed: KEY sits in bits 24-31 and has to be
// 0x26 or the write is discarded, which is why GPIO writes appear to go
// nowhere on a peripheral whose power was never enabled.
#define GPRCM_KEY   0x26000000u

int main(void) {
    // Power up GPIOA before touching it: reset, then enable, then let the
    // enable settle. Until PWREN.ENABLE is set the peripheral's registers are
    // not accessible, so every later write would be lost.
    GPIOA->GPRCM.RSTCTL = GPRCM_KEY
                        | (1u << GPIO_GPRCM_RSTCTL_RESETASSERT_Pos)
                        | (1u << GPIO_GPRCM_RSTCTL_RESETSTKYCLR_Pos);
    GPIOA->GPRCM.PWREN = GPRCM_KEY | (1u << GPIO_GPRCM_PWREN_ENABLE_Pos);
    for (volatile uint32_t settle = 16; settle; settle--) {
    }

    // PA6 as a GPIO output. Pin function 1 is GPIO, and PC has to be set or
    // the pad stays disconnected. The PC encoding is a field value, so it
    // needs shifting into place.
    IOMUX->PINCM[LED_PINCM] = (IOMUX_PINCM_PC_CONNECTED << IOMUX_PINCM_PC_Pos) | 1u;
    GPIOA->DOUTCLR31_0 = LED_PIN;
    GPIOA->DOE31_0 |= LED_PIN;

    systick_init();

    for (;;) {
        GPIOA->DOUTTGL31_0 = LED_PIN;
        delay(500);
    }
}
