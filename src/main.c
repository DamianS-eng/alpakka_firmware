// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include "loop.h"

int main() {
    #ifdef DEVICE_ALPAKKA_V0
        loop_device_init();
    #endif
    #ifdef DEVICE_ALPAKKA_V1
        loop_device_init();
    #endif
    #ifdef DEVICE_DONGLE
        loop_host_init();
    #endif
}
