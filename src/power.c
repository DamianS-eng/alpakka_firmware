// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <pico/time.h>
#include <pico/bootrom.h>
#include <pico/sleep.h>
#include <hardware/watchdog.h>
#include <hardware/gpio.h>
#include <hardware/xosc.h>
#include <hardware/regs/io_bank0.h>
#include "pin.h"
#include "led.h"
#include "esp.h"
#include "imu.h"
#include "loop.h"
#include "logging.h"

void power_restart() {
    watchdog_enable(1, false);  // Reboot after 1 millisecond.
    sleep_ms(10);  // Stall the exexution to avoid resetting the timer.
}

void power_bootsel() {
    if (loop_get_device_mode() == WIRELESS) {
        warn("POWER: Unable to go into bootsel while wireless\n");
        return;
    }
    reset_usb_boot(0, 0);
}

void power_dormant() {
    // Turn off ESP and IMUs.
    esp_enable(false);
    imu_power_off();
    // Turn off leds.
    led_board_set(false);  // USB connector LED.
    led_idle_mask(0b0000);  // Frontal 4 LEDs.
    led_set_mode(LED_MODE_IDLE);
    // Ensure home button is released (after shortcut press).
    while(!gpio_get(PIN_HOME)) {
        sleep_ms(100);
    }
    // In order to go into dormant mode we need to be running from a stoppable
    // clock source (disable USB clock, ADC clock, PLLs).
    sleep_run_from_xosc();
    // Go dormant.
    printf("DORMANT\n");
    sleep_goto_dormant_until_edge_high(PIN_HOME);

    /////////////////////////////////////////////////////////
    //  EXECUTION STOPS HERE UNTIL HOME BUTTON IS PRESSED  //
    /////////////////////////////////////////////////////////

    // Restore clocks.
    sleep_power_up();
    // Force watchdog to restart the controller, so all clocks and magic are
    // guaranteed to go back to defaults (not taking any risk).
    watchdog_enable(10, false);
    sleep_ms(100);
}
