// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

/*
The touch logic determines if the touch surface in the controller is considered
engaged (being touched) or disengaged (not being touched).

The external interface and main function for this feature is "touch_status()",
which returns either true or false (engaged or not) and it is usually called
from the profile(s) layer.

To determine if the surface is touched, the circuit arrangement is composed by
three main elements:
- A GPIO used as pull up or pull down, driving the circuit.
- A GPIO used as input, reading the if the circuit is up (3.3v) or down (GND).
- A monopole capacitor consisting in the trace, the touch plate, and potentially
  the body of the user, so the parasitic capacitance can be measured.

The system measure how long it takes for the circuit to change state, so how
many microseconds passed from the moment the first GPIO was set as pull up/down
to the moment it was confirmed the circuit was effectively driven up/down.

The more parasitic capacitance the circuit has, the slower this process is,
therefore when the user touch the surface, their whole body capacitance makes
the measurement to increase significantly, and can be determined as touched.

The surface is considered touched if the measurement (in microseconds) is bigger
than the defined threshold (also in microseconds).

If the measurement reaches the defined timeout, the surface is also considered
touched.

There are 2 modes of operation:
- Fixed: The threshold is a fixed number of microseconds defined by
  the controller configuration (touch sensitivity preset).
- Dynamic (automatic): The threshold changes dynamically based on the
  measurements from the system, with a relatively simple logic / algorithm.
*/

#include <stdio.h>
#include <math.h>
#include <pico/stdlib.h>
#include "config.h"
#include "touch.h"
#include "pin.h"
#include "common.h"
#include "logging.h"

uint8_t sens_from_config = 0;
float threshold_ratio = 0;
float baseline = 0;

void touch_update_threshold() {
    uint8_t preset = config_get_touch_sens_preset();
    sens_from_config = config_get_touch_sens_value(preset);
    threshold_ratio = TOUCH_AUTO_RATIO;
    if (config_get_pcb_gen() == 0) {
         // PCB gen 0.
        baseline = TOUCH_AUTO_START_GEN0;
    } else {
        // PCB gen 1+.
        baseline = TOUCH_AUTO_START_GEN1;
    }
}

uint8_t touch_get_elapsed() {
    bool timedout = false;
    // Make sure it is settled.
    uint32_t timer_start = time_us_32();
    gpio_put(PIN_TOUCH_OUT, TOUCH_SETTLED_STATE);
    while(gpio_get(PIN_TOUCH_IN) != TOUCH_SETTLED_STATE) {
        if ((time_us_32() - timer_start) > TOUCH_TIMEOUT) {
            timedout = true;
            break;
        }
    }
    // Request up and measure.
    uint32_t timer_settled = time_us_32();
    gpio_put(PIN_TOUCH_OUT, !TOUCH_SETTLED_STATE);
    while(gpio_get(PIN_TOUCH_IN) == TOUCH_SETTLED_STATE) {
        if ((time_us_32() - timer_start) > TOUCH_TIMEOUT) {
            timedout = true;
            break;
        }
    };
    // Request settle for next cycle.
    gpio_put(PIN_TOUCH_OUT, TOUCH_SETTLED_STATE);
    // Calculate elapsed (ignore settling time).
    uint32_t elapsed;
    if (!timedout) elapsed = time_us_32() - timer_settled;
    else elapsed = TOUCH_TIMEOUT;
    return elapsed;
}

// Take as many samples as possible within the available time (timeout).
float touch_get_elapsed_multisample() {
    float total = 0;
    float expected_next = 0;
    uint8_t samples = 0;
    while (expected_next < TOUCH_TIMEOUT) {
        uint8_t elapsed = touch_get_elapsed();
        total += elapsed;
        expected_next = total + elapsed;
        samples++;
    }
    return total / samples;
}

float touch_get_auto_threshold(float elapsed) {
    // Calculate threshold based on current baseline and factor.
    float threshold = baseline * threshold_ratio;
    // Update baseline (with smoothing) if the surface is considered disengaged.
    bool engaged = elapsed >= threshold;
    if (!engaged) {
        baseline = smooth(baseline, elapsed, TOUCH_AUTO_SMOOTH);
    }
    // Return.
    return threshold;
}

bool touch_status() {
    static bool engaged_prev = false;
    static float elapsed_prev = 0;
    // Measure and smooth.
    float elapsed = touch_get_elapsed_multisample();
    float smoothed = (elapsed + elapsed_prev) / 2;
    // Determine threshold.
    float threshold = (
        sens_from_config > 0 ?
        sens_from_config :
        touch_get_auto_threshold(smoothed)
    );
    // Determine if the surface is considered engaged.
    bool engaged = smoothed >= threshold;
    // Periodic debug log.
    if (logging_has_mask(LOG_TOUCH_SENS)) {
        static uint32_t log_last_ts = 0;
        if (time_us_32() > (log_last_ts + (TOUCH_DEBUG_FREQ * 1000))) {
            log_last_ts = time_us_32();
            info("%.1f / %.1f\n", smoothed, threshold);
        }
    }
    // Debug log triggered by state change.
    if (engaged != engaged_prev) {
        if (logging_has_mask(LOG_TOUCH_SENS)) {
            info("%.1f / %.1f", smoothed, threshold);
            if (engaged) info(" TOUCH\n");
            else info(" LIFT\n");
        }
    }
    // Report.
    elapsed_prev = elapsed;
    engaged_prev = engaged;
    return engaged;
}

void touch_log_probe() {
    uint8_t t0 = touch_get_elapsed();
    sleep_ms(CFG_TICK_INTERVAL);
    uint8_t t1 = touch_get_elapsed();
    sleep_ms(CFG_TICK_INTERVAL);
    uint8_t t2 = touch_get_elapsed();
    sleep_ms(CFG_TICK_INTERVAL);
    uint8_t t3 = touch_get_elapsed();
    info("  Touch readings: %ius %ius %ius %ius\n", t0, t1, t2, t3);
}

void touch_init() {
    info("INIT: Touch\n");
    gpio_init(PIN_TOUCH_OUT);
    gpio_set_dir(PIN_TOUCH_OUT, GPIO_OUT);
    gpio_init(PIN_TOUCH_IN);
    gpio_set_dir(PIN_TOUCH_IN, GPIO_IN);
    gpio_set_pulls(PIN_TOUCH_IN, false, false);
    touch_update_threshold();
    touch_log_probe();
}
