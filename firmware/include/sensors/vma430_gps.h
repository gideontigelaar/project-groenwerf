#pragma once
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef struct {
    int      year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    bool     valid;
} utc_time_t;

typedef struct {
    double latitude;
    double longitude;
} location_t;

typedef struct {
    uint8_t  class_byte;
    uint8_t  id_byte;
    uint16_t payload_length;
    uint8_t  CK_A;
    uint8_t  CK_B;
    uint8_t  msg[Config::Gps::UBX_MAX_PAYLOAD];
} ubx_msg_t;

// Populated after gps_parse_ubx_data()
extern utc_time_t utc_time;
extern location_t location;

// Initialise UART and configure the GPS module.
// Returns true on success (ACKs received), false if config commands timed out.
bool gps_init(void);

// Block until a complete UBX packet is received or timeout (~1500 ms).
// Returns true if a valid packet was stored in the internal buffer.
bool gps_get_ubx_packet(void);

// Parse the last received packet into utc_time / location globals.
// Returns true if the packet type was recognised and parsed successfully.
bool gps_parse_ubx_data(void);