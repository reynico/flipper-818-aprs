#pragma once

#include <furi.h>
#include <furi_hal.h>

typedef struct {
    float lat;
    float lon;
    float speed_knots;
    float course;
    float altitude;
    uint8_t sats;
    uint8_t fix_quality;
    bool valid;
    uint32_t last_fix_tick;

    FuriHalSerialHandle *serial;
    FuriStreamBuffer *rx_buf;
    FuriThread *worker;
    volatile bool running;
    uint32_t rx_count;
    uint32_t start_tick;

    char nmea_buf[84];
    uint8_t nmea_len;
} Gps;

void gps_start(Gps *gps);
void gps_stop(Gps *gps);
