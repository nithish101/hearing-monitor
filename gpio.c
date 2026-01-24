/*
 * gpio.c - GPIO hardware control (Raspberry Pi)
 *
 * Controls three indicator LEDs and a piezo buzzer connected to the
 * Raspberry Pi's GPIO header.  Uses the pigpio library for reliable
 * timing and pin control.
 *
 * On non-Pi builds, all functions compile as no-ops.
 *
 * Wiring (BCM pin numbering):
 *   GPIO 17 (pin 11) → Green LED  (anode) → 330Ω → GND
 *   GPIO 27 (pin 13) → Yellow LED (anode) → 330Ω → GND
 *   GPIO 22 (pin 15) → Red LED    (anode) → 330Ω → GND
 *   GPIO 23 (pin 16) → Piezo buzzer (+)   → GND
 */

#ifdef RASPBERRY_PI

#include <stdio.h>
#include <pigpio.h>
#include "gpio.h"

static int gpio_ready = 0;

/* ── Helper: turn all outputs off ───────────────────────────────────── */

static void all_off(void)
{
    gpioWrite(GPIO_LED_GREEN,  0);
    gpioWrite(GPIO_LED_YELLOW, 0);
    gpioWrite(GPIO_LED_RED,    0);
    gpioWrite(GPIO_BUZZER,     0);
}

/* ── Public API ─────────────────────────────────────────────────────── */

int gpio_init(void)
{
    if (gpioInitialise() < 0) {
        fprintf(stderr, "gpio: pigpio init failed (run as root?)\n");
        return -1;
    }

    gpioSetMode(GPIO_LED_GREEN,  PI_OUTPUT);
    gpioSetMode(GPIO_LED_YELLOW, PI_OUTPUT);
    gpioSetMode(GPIO_LED_RED,    PI_OUTPUT);
    gpioSetMode(GPIO_BUZZER,     PI_OUTPUT);

    all_off();
    gpio_ready = 1;
    return 0;
}

void gpio_shutdown(void)
{
    if (gpio_ready) {
        all_off();
        gpioTerminate();
        gpio_ready = 0;
    }
}

void gpio_update(float score)
{
    if (!gpio_ready) return;

    all_off();

    if (score >= 100.0f) {
        /* Threshold reached: red LED + buzzer */
        gpioWrite(GPIO_LED_RED, 1);
        gpioWrite(GPIO_BUZZER,  1);
    } else if (score >= ALERT_DANGER) {
        /* Danger zone: red LED only */
        gpioWrite(GPIO_LED_RED, 1);
    } else if (score >= ALERT_WARNING) {
        /* Warning: yellow LED */
        gpioWrite(GPIO_LED_YELLOW, 1);
    } else {
        /* Safe: green LED */
        gpioWrite(GPIO_LED_GREEN, 1);
    }
}

#else /* !RASPBERRY_PI — stubs */

#include "gpio.h"

int  gpio_init(void)           { return 0; }
void gpio_shutdown(void)       { }
void gpio_update(float score)  { (void)score; }

#endif /* RASPBERRY_PI */
