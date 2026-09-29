#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KISS_FEND 0xC0
#define KISS_FESC 0xDB
#define KISS_TFEND 0xDC
#define KISS_TFESC 0xDD

#define KISS_FRAME_MAX 330
#define KISS_TX_MAX 192

typedef struct
{
    uint8_t buf[KISS_FRAME_MAX + 1];
    uint16_t len;
    bool esc;
    bool overflow;
} KissRx;

uint16_t kiss_encode(const uint8_t *in, uint16_t n, uint8_t *out, uint16_t out_max);
void kiss_rx_reset(KissRx *k);
bool kiss_rx_byte(KissRx *k, uint8_t b);
bool ax25_ui_check(const uint8_t *f, uint16_t n, char *src, uint8_t *src_ssid);
