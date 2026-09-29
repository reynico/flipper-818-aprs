#pragma once

#include "app_state.h"
#include "flipperham.h"

enum
{
    FlipperHamModemProfileDefault = 1,
};

typedef struct
{
    const char *name;
    uint16_t baud;
    uint16_t mark_hz;
    uint16_t space_hz;
} FlipperHamModemProfile;

typedef struct
{
    const char *name;
    char table;
    char code;
} FlipperHamSymbol;

#define WAVE_N 8192

extern const FlipperHamModemProfile flipperham_modem_profiles[2];
extern const FlipperHamSymbol flipperham_symbols[];
extern const uint8_t flipperham_symbols_n;

void txstart(FlipperHamApp *app);
void txstart_raw(FlipperHamApp *app, const uint8_t *ax25, uint16_t n);
bool tx_src(FlipperHamApp *app, const char **src, uint8_t *ssid);
bool tx_my_call(FlipperHamApp *app, char *out, uint8_t n);
