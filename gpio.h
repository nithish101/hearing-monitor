/*
 * gpio.h - GPIO hardware control interface (Raspberry Pi)
 *
 * Provides initialization, shutdown, and LED/buzzer control functions
 * for the physical indicator hardware.  On non-Pi builds these are
 * compiled as no-ops so the rest of the codebase doesn't need guards.
 */

#ifndef GPIO_H
#define GPIO_H

#include "shared.h"

/*
 * gpio_init - initialize GPIO pins for output
 *
 * Sets up the pigpio library and configures the LED and buzzer pins
 * as outputs.  Must be called before any other gpio_* function.
 *
 * @return  0 on success, -1 on failure
 */
int gpio_init(void);

/*
 * gpio_shutdown - release GPIO resources
 *
 * Turns off all outputs and terminates the pigpio library.
 * Safe to call even if gpio_init was never called or failed.
 */
void gpio_shutdown(void);

/*
 * gpio_update - set LED and buzzer state based on the current strain score
 *
 * Score ranges:
 *   [ 0, 60)  → green LED on, others off
 *   [60, 80)  → yellow LED on, others off
 *   [80,100)  → red LED on, others off
 *   [100]     → red LED on + buzzer pulsing
 *
 * @param score  Current strain score (0–100)
 */
void gpio_update(float score);

#endif /* GPIO_H */
