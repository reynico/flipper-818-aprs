#include "kiss.h"

#include <string.h>

uint16_t kiss_encode(const uint8_t *in, uint16_t n, uint8_t *out, uint16_t out_max)
{
    uint16_t i;
    uint16_t o = 0;

    if (!in || !out || out_max < 3)
        return 0;

    out[o++] = KISS_FEND;
    out[o++] = 0x00;
    for (i = 0; i < n; i++)
    {
        if (in[i] == KISS_FEND || in[i] == KISS_FESC)
        {
            if (o + 2 >= out_max)
                return 0;
            out[o++] = KISS_FESC;
            out[o++] = in[i] == KISS_FEND ? KISS_TFEND : KISS_TFESC;
        }
        else
        {
            if (o + 1 >= out_max)
                return 0;
            out[o++] = in[i];
        }
    }
    out[o++] = KISS_FEND;
    return o;
}

void kiss_rx_reset(KissRx *k)
{
    k->len = 0;
    k->esc = false;
    k->overflow = false;
}

bool kiss_rx_byte(KissRx *k, uint8_t b)
{
    if (b == KISS_FEND)
    {
        bool done = k->len > 1 && !k->overflow && !k->esc;
        if (!done)
            kiss_rx_reset(k);
        return done;
    }

    if (k->esc)
    {
        k->esc = false;
        if (b == KISS_TFEND)
            b = KISS_FEND;
        else if (b == KISS_TFESC)
            b = KISS_FESC;
        else
        {
            k->overflow = true;
            return false;
        }
    }
    else if (b == KISS_FESC)
    {
        k->esc = true;
        return false;
    }

    if (k->len >= sizeof(k->buf))
    {
        k->overflow = true;
        return false;
    }
    k->buf[k->len++] = b;
    return false;
}

static bool ax25_addr_ok(const uint8_t *a, char *call, uint8_t *ssid)
{
    uint8_t i;
    uint8_t n = 0;
    bool pad = false;
    char c;

    for (i = 0; i < 6; i++)
    {
        if (a[i] & 1)
            return false;
        c = (char)(a[i] >> 1);
        if (c == ' ')
        {
            pad = true;
            continue;
        }
        if (pad || !((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
        if (call)
            call[n] = c;
        n++;
    }
    if (!n)
        return false;
    if (call)
        call[n] = 0;
    if (ssid)
        *ssid = (a[6] >> 1) & 0x0F;
    return true;
}

bool ax25_ui_check(const uint8_t *f, uint16_t n, char *src, uint8_t *src_ssid)
{
    uint16_t pos;
    uint8_t k;

    if (!f || n < 16 || n > KISS_TX_MAX)
        return false;
    if (!ax25_addr_ok(f, NULL, NULL) || (f[6] & 1))
        return false;
    if (!ax25_addr_ok(f + 7, src, src_ssid))
        return false;

    pos = 14;
    k = 0;
    if (!(f[13] & 1))
    {
        while (1)
        {
            if (pos + 7 > n || k >= 8)
                return false;
            if (!ax25_addr_ok(f + pos, NULL, NULL))
                return false;
            if (f[pos + 6] & 0x80)
                return false;
            k++;
            pos += 7;
            if (f[pos - 1] & 1)
                break;
        }
    }

    if (pos + 2 > n)
        return false;
    return f[pos] == 0x03 && f[pos + 1] == 0xF0;
}
