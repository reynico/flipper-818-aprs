#include "dra818v.h"

#include <stdio.h>
#include <string.h>

static void serial_rx_cb(
    FuriHalSerialHandle *handle,
    FuriHalSerialRxEvent event,
    void *ctx)
{
    Dra818v *dra = ctx;

    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(dra->rx_buf, &byte, 1, 0);
    }
}

static bool send_cmd(Dra818v *dra, const char *cmd, char *resp, size_t resp_size)
{
    uint8_t drain;
    size_t pos = 0;
    uint32_t start;

    while(furi_stream_buffer_receive(dra->rx_buf, &drain, 1, 0) > 0)
        ;

    furi_hal_serial_tx(dra->serial, (const uint8_t *)cmd, strlen(cmd));

    start = furi_get_tick();
    while(furi_get_tick() - start < 2000) {
        uint8_t byte;
        if(furi_stream_buffer_receive(dra->rx_buf, &byte, 1, 100) > 0) {
            if(pos < resp_size - 1)
                resp[pos++] = byte;
            if(byte == '\n')
                break;
        }
    }
    resp[pos] = 0;
    return pos > 0;
}

bool dra818v_init(Dra818v *dra)
{
    dra->rx_buf = furi_stream_buffer_alloc(128, 1);
    dra->ready = false;

    furi_hal_gpio_init(dra->ptt_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(dra->pd_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(dra->ptt_pin, true);
    furi_hal_gpio_write(dra->pd_pin, true);

    if(dra->sq_pin)
        furi_hal_gpio_init(dra->sq_pin, GpioModeInput, GpioPullDown, GpioSpeedLow);

    dra->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!dra->serial) {
        furi_stream_buffer_free(dra->rx_buf);
        return false;
    }

    furi_hal_serial_init(dra->serial, DRA818V_BAUD);
    furi_hal_serial_async_rx_start(dra->serial, serial_rx_cb, dra, false);

    furi_delay_ms(500);
    dra->ready = true;
    return true;
}

void dra818v_deinit(Dra818v *dra)
{
    if(!dra->ready) return;

    dra818v_ptt_off(dra);
    furi_hal_gpio_write(dra->pd_pin, false);

    furi_hal_serial_async_rx_stop(dra->serial);
    furi_hal_serial_deinit(dra->serial);
    furi_hal_serial_control_release(dra->serial);
    dra->serial = NULL;

    furi_hal_gpio_init(dra->ptt_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(dra->pd_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    if(dra->sq_pin)
        furi_hal_gpio_init(dra->sq_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    furi_stream_buffer_free(dra->rx_buf);
    dra->ready = false;
}

bool dra818v_handshake(Dra818v *dra)
{
    char resp[DRA818V_RESP_MAX];
    if(!send_cmd(dra, "AT+DMOCONNECT\r\n", resp, sizeof(resp)))
        return false;
    return strstr(resp, "+DMOCONNECT:0") != NULL;
}

bool dra818v_set_group(Dra818v *dra, float tx_freq, float rx_freq, uint8_t sq)
{
    char cmd[DRA818V_CMD_MAX];
    char resp[DRA818V_RESP_MAX];

    if(sq > 8) sq = 8;
    snprintf(
        cmd, sizeof(cmd),
        "AT+DMOSETGROUP=0,%.4f,%.4f,0000,%u,0000\r\n",
        (double)tx_freq, (double)rx_freq, sq);

    if(!send_cmd(dra, cmd, resp, sizeof(resp)))
        return false;
    return strstr(resp, "+DMOSETGROUP:0") != NULL;
}

bool dra818v_set_volume(Dra818v *dra, uint8_t vol)
{
    char cmd[DRA818V_CMD_MAX];
    char resp[DRA818V_RESP_MAX];

    if(vol < 1) vol = 1;
    if(vol > 8) vol = 8;
    snprintf(cmd, sizeof(cmd), "AT+DMOSETVOLUME=%u\r\n", vol);

    if(!send_cmd(dra, cmd, resp, sizeof(resp)))
        return false;
    return strstr(resp, "+DMOSETVOLUME:0") != NULL;
}

bool dra818v_set_filter(Dra818v *dra, bool pre_emph, bool highpass, bool lowpass)
{
    char cmd[DRA818V_CMD_MAX];
    char resp[DRA818V_RESP_MAX];

    snprintf(
        cmd, sizeof(cmd),
        "AT+SETFILTER=%u,%u,%u\r\n",
        pre_emph ? 1 : 0, highpass ? 1 : 0, lowpass ? 1 : 0);

    if(!send_cmd(dra, cmd, resp, sizeof(resp)))
        return false;
    return strstr(resp, "+DMOSETFILTER:0") != NULL;
}

void dra818v_ptt_on(Dra818v *dra)
{
    furi_hal_gpio_write(dra->ptt_pin, false);
}

void dra818v_ptt_off(Dra818v *dra)
{
    furi_hal_gpio_write(dra->ptt_pin, true);
}

bool dra818v_send_raw(Dra818v *dra, const char *cmd, char *resp, size_t resp_size)
{
    return send_cmd(dra, cmd, resp, resp_size);
}

bool dra818v_squelch_open(Dra818v *dra)
{
    if(!dra->sq_pin) return true;
    return furi_hal_gpio_read(dra->sq_pin);
}
