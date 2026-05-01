#include "afsk.h"
#include "ax25_decode.h"

#include <furi_hal_bus.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_resources.h>
#include <furi_hal_adc.h>
#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_adc.h>

#include <string.h>

#define AFSK_RX_FLAG_SAMPLE 1
#define AFSK_RX_FLAG_STOP   2

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
#define AFSK_NOISE_FLOOR 500.0f

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
            if(rx->state == AfskRxData && rx->frame_len >= 17) {
                rx->dbg_last_frame_len = rx->frame_len;
                AfskFrame frame;
                if(ax25_decode_frame(rx->frame_buf, rx->frame_len, &frame)) {
                    if(rx->frame_cb)
                        rx->frame_cb(&frame, rx->frame_ctx);
                } else {
                    rx->dbg_crc_fail++;
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

/* ── RX: TIM2 ISR collects hardware-timed ADC samples ──────────── */

static void afsk_rx_isr(void *ctx)
{
    AfskRx *rx = ctx;
    LL_TIM_ClearFlag_UPDATE(TIM2);

    if(!rx->running || !rx->worker_id) return;

    for(uint32_t t = 0; t < 100 && !LL_ADC_IsActiveFlag_EOC(ADC1); t++) {}
    if(!LL_ADC_IsActiveFlag_EOC(ADC1)) {
        rx->dbg_adc_timeout++;
        return;
    }

    uint16_t raw = LL_ADC_REG_ReadConversionData12(ADC1);
    LL_ADC_ClearFlag_EOC(ADC1);

    uint16_t wi = rx->wr;
    rx->samples[wi & AFSK_RX_BUF_MASK] = (int16_t)raw;
    rx->wr = wi + 1;

    if((wi & 0x3F) == 0)
        furi_thread_flags_set(rx->worker_id, AFSK_RX_FLAG_SAMPLE);
}

/* ── RX: worker thread drains buffer and demodulates ───────────── */

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
    bool carrier_present = false;
    uint32_t silence_count = 0;

    while(rx->running) {
        uint32_t flags = furi_thread_flags_wait(
            AFSK_RX_FLAG_SAMPLE | AFSK_RX_FLAG_STOP,
            FuriFlagWaitAny, FuriWaitForever);

        if(flags == (uint32_t)FuriFlagErrorTimeout) continue;
        if(flags & FuriFlagError) break;
        if(flags & AFSK_RX_FLAG_STOP) break;

        uint16_t avail = rx->wr - rx->rd;
        if(avail >= AFSK_RX_BUF_SIZE) {
            rx->rd = rx->wr - AFSK_RX_BUF_SIZE / 2;
            rx->dbg_overruns++;
        }

        uint16_t wi = rx->wr;
        while(rx->rd != wi) {
            int16_t raw = rx->samples[rx->rd & AFSK_RX_BUF_MASK];
            rx->rd++;

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

            float mag = lpf < 0 ? -lpf : lpf;
            if(mag > AFSK_NOISE_FLOOR) {
                carrier_present = true;
                silence_count = 0;
            } else {
                silence_count++;
                if(silence_count > AFSK_SAMPLES_PER_BIT * 20) {
                    carrier_present = false;
                    rx->state = AfskRxHunt;
                }
            }

            bool cur_sign = lpf > 0;
            if(cur_sign != last_sign) {
                if(bit_phase < 4)
                    bit_phase += 2;
                else if(bit_phase > 7)
                    bit_phase -= 2;
                last_sign = cur_sign;
            }

            bit_phase++;
            if(bit_phase >= AFSK_SAMPLES_PER_BIT) {
                bit_phase = 0;

                rx->dbg_space = mag;

                if(carrier_present) {
                    bool is_mark = lpf < 0;
                    uint8_t bit = (is_mark == rx->last_tone) ? 1 : 0;
                    rx->last_tone = is_mark;

                    if(rx->ones_count == 6 && !bit)
                        rx->dbg_flags++;

                    rx_process_bit(rx, bit);
                }
            }
        }
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

    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_11);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_11, LL_ADC_SAMPLINGTIME_12CYCLES_5);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_EXT_TIM2_TRGO);
    LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_RISING);
    LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);

    rx->running = true;
    rx->worker = furi_thread_alloc_ex("afsk_rx", 4096, afsk_rx_worker, rx);
    furi_thread_set_priority(rx->worker, FuriThreadPriorityNormal);
    furi_thread_start(rx->worker);
    rx->worker_id = furi_thread_get_id(rx->worker);

    furi_hal_bus_enable(FuriHalBusTIM2);
    LL_TIM_SetPrescaler(TIM2, 0);
    LL_TIM_SetCounterMode(TIM2, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(TIM2, (SystemCoreClock / AFSK_SAMPLE_RATE) - 1);
    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_UPDATE);
    LL_TIM_GenerateEvent_UPDATE(TIM2);

    LL_TIM_ClearFlag_UPDATE(TIM2);
    LL_TIM_SetCounter(TIM2, 0);
    LL_ADC_ClearFlag_EOC(ADC1);
    LL_ADC_ClearFlag_OVR(ADC1);

    LL_ADC_REG_StartConversion(ADC1);

    LL_TIM_EnableIT_UPDATE(TIM2);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, afsk_rx_isr, rx);
    LL_TIM_EnableCounter(TIM2);
}

void afsk_rx_stop(AfskRx *rx)
{
    rx->running = false;

    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableIT_UPDATE(TIM2);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, NULL, NULL);

    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);

    if(furi_hal_bus_is_enabled(FuriHalBusTIM2))
        furi_hal_bus_disable(FuriHalBusTIM2);

    if(LL_ADC_REG_IsConversionOngoing(ADC1))
        LL_ADC_REG_StopConversion(ADC1);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    LL_ADC_ClearFlag_EOC(ADC1);
    LL_ADC_ClearFlag_OVR(ADC1);

    furi_thread_flags_set(rx->worker_id, AFSK_RX_FLAG_STOP);
    furi_thread_join(rx->worker);
    furi_thread_free(rx->worker);
    rx->worker = NULL;
    rx->worker_id = 0;

    furi_hal_adc_release(rx->adc);
    rx->adc = NULL;

    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
