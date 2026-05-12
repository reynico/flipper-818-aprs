#pragma once

#include <furi.h>
#include <furi_hal.h>

#define DRA818V_BAUD 9600
#define DRA818V_RESP_MAX 48
#define DRA818V_CMD_MAX 64

typedef struct {
    FuriHalSerialHandle *serial;
    const GpioPin *ptt_pin;
    const GpioPin *pd_pin;
    const GpioPin *sq_pin;
    const GpioPin *hl_pin;
    FuriStreamBuffer *rx_buf;
    bool ready;
    bool high_power;
} Dra818v;

bool dra818v_init(Dra818v *dra);
void dra818v_deinit(Dra818v *dra);
bool dra818v_handshake(Dra818v *dra);
bool dra818v_set_group(Dra818v *dra, float tx_freq, float rx_freq, uint8_t sq);
bool dra818v_set_volume(Dra818v *dra, uint8_t vol);
bool dra818v_set_filter(Dra818v *dra, bool pre_emph, bool highpass, bool lowpass);
void dra818v_set_power(Dra818v *dra, bool high_power);
void dra818v_ptt_on(Dra818v *dra);
void dra818v_ptt_off(Dra818v *dra);
bool dra818v_squelch_open(Dra818v *dra);
bool dra818v_send_raw(Dra818v *dra, const char *cmd, char *resp, size_t resp_size);
