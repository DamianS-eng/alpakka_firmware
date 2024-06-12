// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <string.h>
#include <pico/time.h>
#include "wireless.h"
#include "pin.h"
#include "led.h"
#include "bus.h"
#include "hid.h"
#include "loop.h"
#include "logging.h"
#include "common.h"

static void led_task() {
    static uint8_t i = 0;
    static bool led_state;
    i++;
    if (i==100) {
        led_board_set(led_state);
        led_state = !led_state;
        i = 0;
    }
}

void wireless_send(uint8_t report_id, void *packet, uint8_t len) {
    // uint8_t payload[32] = {0,};
    // payload[0] = report_id;
    // memcpy(&payload[1], packet, len);
    // bus_spi_write_32(13, NRF24_W_TX_PAYLOAD, payload);
}

void wireless_device_init() {
    printf("INIT: RF device\n");
    uint8_t config_set = NRF24_REG_CONFIG_PWR_UP | NRF24_REG_CONFIG_EN_CRC;
    bus_spi_write(PIN_SPI_CS_NRF24, NRF24_REG_W | NRF24_REG_CONFIG, config_set);
    uint8_t config_get = bus_spi_read_one(PIN_SPI_CS_NRF24, NRF24_REG_CONFIG);
    if (config_get != config_set) error( "RF: NRF24 configuration mismatch\n");
    printf("RF: config=%i\n", config_get);
}

void wireless_host_init() {
    printf("INIT: RF host\n");
    // Set config.
    uint8_t config_set = NRF24_REG_CONFIG_PWR_UP | NRF24_REG_CONFIG_EN_CRC | NRF24_REG_CONFIG_PRIM_RX;
    bus_spi_write(PIN_SPI_CS_NRF24, NRF24_REG_W | NRF24_REG_CONFIG, config_set);
    uint8_t config_get = bus_spi_read_one(PIN_SPI_CS_NRF24, NRF24_REG_CONFIG);
    if (config_get != config_set) error( "RF: NRF24 configuration mismatch\n");
    // Set payload size
    bus_spi_write(PIN_SPI_CS_NRF24, NRF24_REG_W | NRF24_REG_RX_PW_P0, 32);
    uint8_t payload_size = bus_spi_read_one(PIN_SPI_CS_NRF24, NRF24_REG_RX_PW_P0);
    printf("RF: config=%i\n", config_get);
    printf("RF: payload_size=%i\n", payload_size);
}

void wireless_device_task() {
    led_task();

    static uint8_t index = 0;
    index++;

    bus_spi_write_32(PIN_SPI_CS_NRF24, NRF24_REG_W | NRF24_REG_STATUS, 0);

    // uint8_t payload[32] = {0,};
    // uint64_t timestamp = get_system_clock();
    // memcpy(payload, &timestamp, 8);

    uint8_t payload[32] = {index};

    bus_spi_write_32(PIN_SPI_CS_NRF24, NRF24_W_TX_PAYLOAD, payload);
    // printf("%i %i %i %i\n", payload[0], payload[1], payload[2], payload[3]);

    // uint8_t status = bus_spi_read_one(PIN_SPI_CS_NRF24, NRF24_REG_STATUS);
    // printf("0b%08i\n", bin(status));

    // uint8_t observe = bus_spi_read_one(PIN_SPI_CS_NRF24, NRF24_REG_OBSERVE_TX);
    // printf("0b%08i\n", bin(observe));
}

void wireless_host_task() {
    led_task();
    while(true) {
        uint8_t status = bus_spi_read_one(PIN_SPI_CS_NRF24, NRF24_REG_STATUS);
        if (status & 0b10) break;  // No payloads pending in pipe 0.

        uint8_t payload[32] = {0,};
        bus_spi_read(PIN_SPI_CS_NRF24, NRF24_R_RX_PAYLOAD, payload, 32);
        // printf("%i %i %i %i\n", payload[0], payload[1], payload[2], payload[3]);

        // Jitter.
        static uint32_t last = 0;
        uint32_t now = time_us_32() / 1000;
        uint32_t elapsed = now - last;
        last = now;
        printf("%lu ", elapsed);

        // Latency.
        // uint64_t now = get_system_clock();
        // uint64_t timestamp;
        // memcpy(&timestamp, payload, 8);
        // uint64_t latency = now - timestamp;
        // static double latency_sum = 0;
        // static uint16_t i = 0;
        // latency_sum += latency;
        // i++;
        // if (i == 500) {
        //     printf("latency_avg=%f ms\n", latency_sum/500);
        //     latency_sum = 0;
        //     i=0;
        // }
    }
}
