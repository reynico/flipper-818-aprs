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

/* ── RX: delay-and-multiply tone discriminator ──────────────────── */

#define AFSK_DELAY       6
#define AFSK_LPF_ALPHA   0.30f
#define AFSK_NOISE_FLOOR 5000.0f

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
    int16_t delay_line[AFSK_DELAY] = {0};
    uint8_t delay_idx = 0;
    float lpf = 0;
    float dc_avg = 2048.0f;
    bool last_sign = false;
    uint8_t bit_phase = 0;
    int16_t adc_min = 32767, adc_max = -32768;
    uint32_t sample_n = 0;

    while(rx->running) {
        furi_delay_us(76);
        uint16_t raw = furi_hal_adc_read(rx->adc, FuriHalAdcChannel11);
        dc_avg = dc_avg * 0.995f + (float)raw * 0.005f;
        int16_t x = (int16_t)((float)raw - dc_avg);

        if(x < adc_min) adc_min = x;
        if(x > adc_max) adc_max = x;
        sample_n++;
        if((sample_n & 0xFF) == 0) {
            rx->dbg_adc_min = adc_min;
            rx->dbg_adc_max = adc_max;
            adc_min = 32767;
            adc_max = -32768;
        }

        float product = (float)x * (float)delay_line[delay_idx];
        delay_line[delay_idx] = x;
        delay_idx++;
        if(delay_idx >= AFSK_DELAY) delay_idx = 0;

        lpf = lpf * (1.0f - AFSK_LPF_ALPHA) + product * AFSK_LPF_ALPHA;
        rx->dbg_mark = lpf;

        bool cur_sign = lpf > 0;
        if(cur_sign != last_sign) {
            if(bit_phase < (AFSK_SAMPLES_PER_BIT / 2))
                bit_phase++;
            else if(bit_phase > (AFSK_SAMPLES_PER_BIT / 2 + 1))
                bit_phase--;
            last_sign = cur_sign;
        }

        bit_phase++;
        if(bit_phase >= AFSK_SAMPLES_PER_BIT) {
            bit_phase = 0;

            float mag = lpf < 0 ? -lpf : lpf;
            rx->dbg_space = mag;

            if(mag > AFSK_NOISE_FLOOR) {
                bool is_mark = lpf < 0;
                uint8_t bit = (is_mark == rx->last_tone) ? 1 : 0;
                rx->last_tone = is_mark;

                if(rx->ones_count == 6 && !bit)
                    rx->dbg_flags++;

                rx_process_bit(rx, bit);
            }
        }

        if((sample_n & 0xF) == 0)
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
