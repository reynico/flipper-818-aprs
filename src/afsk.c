#include "afsk.h"
#include "ax25_decode.h"

#include <furi_hal_bus.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_resources.h>
#include <furi_hal_adc.h>
#include <stm32wbxx_ll_tim.h>

#include <string.h>

/* ── TX: timer ISR toggles GPIO at waveform edge durations ──────── */

static void afsk_tx_isr(void *ctx)
{
    AfskTx *tx = ctx;
    LL_TIM_ClearFlag_UPDATE(TIM2);

    if(!tx->active) return;

    tx->pos++;
    if(tx->pos >= tx->wave_len) {
        tx->active = false;
        return;
    }

    tx->level = !tx->level;
    furi_hal_gpio_write(&gpio_ext_pa4, tx->level);

    uint32_t ticks = (uint32_t)tx->wave[tx->pos] * (SystemCoreClock / 1000000);
    if(ticks < 2) ticks = 2;
    LL_TIM_SetAutoReload(TIM2, ticks - 1);
}

void afsk_tx_start(AfskTx *tx, uint16_t *wave, uint16_t wave_len)
{
    uint32_t ticks;

    tx->wave = wave;
    tx->wave_len = wave_len;
    tx->pos = 0;
    tx->level = true;

    if(!wave_len) return;

    furi_hal_gpio_init(
        &gpio_ext_pa4, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(&gpio_ext_pa4, tx->level);

    furi_hal_bus_enable(FuriHalBusTIM2);
    LL_TIM_SetPrescaler(TIM2, 0);
    LL_TIM_SetCounterMode(TIM2, LL_TIM_COUNTERMODE_UP);

    ticks = (uint32_t)wave[0] * (SystemCoreClock / 1000000);
    if(ticks < 2) ticks = 2;
    LL_TIM_SetAutoReload(TIM2, ticks - 1);
    LL_TIM_GenerateEvent_UPDATE(TIM2);
    LL_TIM_EnableIT_UPDATE(TIM2);

    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, afsk_tx_isr, tx);

    tx->active = true;
    LL_TIM_EnableCounter(TIM2);
}

void afsk_tx_stop(AfskTx *tx)
{
    tx->active = false;

    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableIT_UPDATE(TIM2);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, NULL, NULL);

    if(furi_hal_bus_is_enabled(FuriHalBusTIM2))
        furi_hal_bus_disable(FuriHalBusTIM2);

    furi_hal_gpio_init(&gpio_ext_pa4, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

/* ── RX: Goertzel tone detection ────────────────────────────────── */

static const float goertzel_coeff_mark = 1.68251f;
static const float goertzel_coeff_space = 1.0f;

static float goertzel_mag(const int16_t *buf, uint8_t n, float coeff)
{
    float s1 = 0, s2 = 0;

    for(uint8_t i = 0; i < n; i++) {
        float s0 = (float)buf[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

/* ── RX: bit-level AX.25 state machine ─────────────────────────── */

static void rx_process_bit(AfskRx *rx, uint8_t bit)
{
    if(bit) {
        rx->ones_count++;
        if(rx->ones_count > 6) {
            rx->state = AfskRxHunt;
            rx->ones_count = 0;
            return;
        }
        if(rx->ones_count == 6)
            return;
    } else {
        if(rx->ones_count == 6) {
            if(rx->state == AfskRxData && rx->frame_len >= 17 && rx->bit_pos == 0) {
                AfskFrame frame;
                if(ax25_decode_frame(rx->frame_buf, rx->frame_len, &frame)) {
                    if(rx->frame_cb)
                        rx->frame_cb(&frame, rx->frame_ctx);
                }
            }
            rx->state = AfskRxData;
            rx->frame_len = 0;
            rx->byte_acc = 0;
            rx->bit_pos = 0;
            rx->ones_count = 0;
            return;
        }
        if(rx->ones_count == 5) {
            rx->ones_count = 0;
            return;
        }
        rx->ones_count = 0;
    }

    if(rx->state != AfskRxData) return;

    rx->byte_acc |= (bit << rx->bit_pos);
    rx->bit_pos++;

    if(rx->bit_pos == 8) {
        if(rx->frame_len < AFSK_RX_FRAME_MAX)
            rx->frame_buf[rx->frame_len++] = rx->byte_acc;
        rx->byte_acc = 0;
        rx->bit_pos = 0;
    }
}

/* ── RX: worker thread samples ADC and demodulates ──────────────── */

static int32_t afsk_rx_worker(void *ctx)
{
    AfskRx *rx = ctx;
    int16_t block[AFSK_SAMPLES_PER_BIT];
    int16_t adc_min = 32767, adc_max = -32768;
    float dc_avg = 2048.0f;
    uint32_t sample_n = 0;

    while(rx->running) {
        for(uint8_t i = 0; i < AFSK_SAMPLES_PER_BIT; i++) {
            furi_delay_us(76);
            uint16_t raw = furi_hal_adc_read(rx->adc, FuriHalAdcChannel11);
            dc_avg = dc_avg * 0.995f + (float)raw * 0.005f;
            int16_t centered = (int16_t)((float)raw - dc_avg);
            block[i] = centered;
            if(centered < adc_min) adc_min = centered;
            if(centered > adc_max) adc_max = centered;
        }

        sample_n++;
        if((sample_n & 0x3F) == 0) {
            rx->dbg_adc_min = adc_min;
            rx->dbg_adc_max = adc_max;
            adc_min = 32767;
            adc_max = -32768;
        }

        float m_mark = goertzel_mag(block, AFSK_SAMPLES_PER_BIT, goertzel_coeff_mark);
        float m_space = goertzel_mag(block, AFSK_SAMPLES_PER_BIT, goertzel_coeff_space);
        rx->dbg_mark = m_mark;
        rx->dbg_space = m_space;
        bool is_mark = m_mark > m_space;

        uint8_t bit = (is_mark == rx->last_tone) ? 1 : 0;
        rx->last_tone = is_mark;

        if(rx->ones_count == 6 && !bit)
            rx->dbg_flags++;

        rx_process_bit(rx, bit);
        furi_thread_yield();
    }

    return 0;
}

/* ── RX: start / stop ───────────────────────────────────────────── */

void afsk_rx_start(AfskRx *rx, void (*cb)(AfskFrame *, void *), void *ctx)
{
    memset(rx, 0, sizeof(AfskRx));
    rx->frame_cb = cb;
    rx->frame_ctx = ctx;
    rx->last_tone = true;
    rx->state = AfskRxHunt;

    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    rx->adc = furi_hal_adc_acquire();
    furi_hal_adc_configure_ex(
        rx->adc,
        FuriHalAdcScale2500,
        FuriHalAdcClockSync64,
        FuriHalAdcOversampleNone,
        FuriHalAdcSamplingtime12_5);

    rx->running = true;
    rx->worker = furi_thread_alloc_ex("afsk_rx", 4096, afsk_rx_worker, rx);
    furi_thread_set_priority(rx->worker, FuriThreadPriorityNormal);
    furi_thread_start(rx->worker);
}

void afsk_rx_stop(AfskRx *rx)
{
    rx->running = false;
    furi_thread_join(rx->worker);
    furi_thread_free(rx->worker);
    rx->worker = NULL;

    furi_hal_adc_release(rx->adc);
    rx->adc = NULL;

    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
