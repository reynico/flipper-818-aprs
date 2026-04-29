#pragma once

#include "afsk.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char src[10];
    char dst[10];
    char path[56];
    uint8_t type;
    float lat;
    float lon;
    char symbol[3];
    char comment[128];
    char msg_to[10];
    char msg_text[68];
    bool has_pos;
    bool has_msg;
} AprsDecoded;

bool ax25_decode_frame(const uint8_t *buf, uint16_t len, AfskFrame *out);
bool aprs_decode(const AfskFrame *frame, AprsDecoded *out);
