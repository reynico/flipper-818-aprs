#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>

#define AFSK_SAMPLE_RATE     13200
#define AFSK_MARK_HZ         1200
#define AFSK_SPACE_HZ        2200
#define AFSK_BAUD            1200
#define AFSK_SAMPLES_PER_BIT (AFSK_SAMPLE_RATE / AFSK_BAUD)

#define AFSK_RX_BUF_BITS     8
#define AFSK_RX_BUF_SIZE     (1 << AFSK_RX_BUF_BITS)
#define AFSK_RX_BUF_MASK     (AFSK_RX_BUF_SIZE - 1)
#define AFSK_RX_FRAME_MAX    330

typedef struct {
    char src[10];
    uint8_t src_ssid;
    char dst[10];
    uint8_t dst_ssid;
    char path[56];
    uint8_t payload[256];
    uint16_t payload_len;
    bool valid;
} AfskFrame;

typedef struct {
    uint16_t *wave;
    uint16_t wave_len;
    volatile uint16_t pos;
    volatile bool level;
    volatile bool active;
} AfskTx;

typedef enum {
    AfskRxHunt,
    AfskRxData,
} AfskRxState;

typedef struct {
    volatile int16_t samples[AFSK_RX_BUF_SIZE];
    volatile uint16_t wr;
    uint16_t rd;

    int16_t block[AFSK_SAMPLES_PER_BIT];

    bool last_tone;
    AfskRxState state;
    uint8_t ones_count;
    uint8_t byte_acc;
    uint8_t bit_pos;
    uint8_t frame_buf[AFSK_RX_FRAME_MAX];
    uint16_t frame_len;

    void (*frame_cb)(AfskFrame *frame, void *ctx);
    void *frame_ctx;

    FuriHalAdcHandle *adc;
    volatile bool running;
    FuriThread *worker;

    volatile int16_t dbg_adc_min;
    volatile int16_t dbg_adc_max;
    volatile float dbg_mark;
    volatile float dbg_space;
    volatile uint32_t dbg_flags;
} AfskRx;

void afsk_tx_start(AfskTx *tx, uint16_t *wave, uint16_t wave_len);
void afsk_tx_stop(AfskTx *tx);

void afsk_rx_start(AfskRx *rx, void (*cb)(AfskFrame *, void *), void *ctx);
void afsk_rx_stop(AfskRx *rx);
