#include "rf_gen.h"
#include "aprs.h"

#include <furi_hal.h>

#include <stdio.h>
#include <string.h>

bool call_split(const char *s, char *out, uint8_t *ssid, bool *has_ssid);

static bool wave_flag(FlipperHamApp *app);
static bool wave_put(FlipperHamApp *app, uint8_t bit);
static bool wave_add(FlipperHamApp *app, double value);
static uint16_t round_u16_even(double value);
static const char *aprs_path_pick(FlipperHamApp *app);

const FlipperHamModemProfile flipperham_modem_profiles[] = {
    {"300bd", 300, 1600, 1800},
    {"1200bd", 1200, 1200, 2200},
};

static const char *aprs_path_pick(FlipperHamApp *app)
{
    static const char *paths[] = {"None", "RFONLY", "NOGATE", "WIDE1-1", "WIDE2-2", "ARISS", "APRSAT", "Custom"};

    if (!app) return NULL;
    if (app->aprs_path_index >= sizeof(paths) / sizeof(paths[0])) return NULL;
    if (app->aprs_path_index == 0) return NULL;
    if (app->aprs_path_index == 7 && app->aprs_path_edit[0]) return app->aprs_path_edit;
    if (app->aprs_path_index == 7) return NULL;


    return paths[app->aprs_path_index];
}

static bool wave_add(FlipperHamApp *app, double value)
{
    uint16_t pulse;

    if (!app->wave)
        return false;
    if (app->wave_len >= WAVE_N)
        return false;

    value += app->wave_carry;
    pulse = round_u16_even(value);
    app->wave_carry = value - pulse;
    if (!pulse)
        return true;

    app->wave[app->wave_len++] = pulse;
    return true;
}

static uint16_t round_u16_even(double value)
{
    uint16_t whole;
    double frac;

    whole = (uint16_t)value;
    frac = value - whole;

    if (frac > (double)0.5f)
        whole++;
    else if (frac >= (double)0.5f - (double)0.000001f)
        if (whole & 1)
            whole++;

    return whole;
}

static bool wave_put(FlipperHamApp *app, uint8_t bit)
{
    const FlipperHamModemProfile *profile;
    double bit_us;
    double half_us;
    double accum_us;

    profile = &flipperham_modem_profiles[1];
    bit_us = 1000000.0 / profile->baud;

    if (bit == 0)
    {
        app->wave_is_mark = !app->wave_is_mark;
    }

    if (app->wave_is_mark)
        half_us = 1000000.0 / (2.0 * profile->mark_hz);
    else
        half_us = 1000000.0 / (2.0 * profile->space_hz);

    if (app->wave_prev_h <= (double)0.000001f)
    {
        app->wave_prev_h = half_us;
        app->wave_pending = 0;
        app->wave_osc_remain = half_us;
    }
    else if (half_us < app->wave_prev_h - (double)0.000001f ||
             half_us > app->wave_prev_h + (double)0.000001f)
    {
        if (app->wave_pending > (double)0.000001f)
        {
            if (!wave_add(app, app->wave_pending))
                return false;
        }

        app->wave_prev_h = half_us;
        app->wave_pending = 0;
        app->wave_osc_remain = half_us;
    }

    accum_us = app->wave_pending + bit_us;

    while (accum_us >= app->wave_prev_h - (double)0.000001f)
    {
        if (!wave_add(app, app->wave_prev_h))
            return false;
        accum_us -= app->wave_prev_h;
    }

    app->wave_pending = accum_us;
    app->wave_osc_remain = app->wave_prev_h - accum_us;

    return true;
}

static bool wave_flag(FlipperHamApp *app)
{
    static const uint8_t flag[] = {0, 1, 1, 1, 1, 1, 1, 0};
    uint8_t i;

    for (i = 0; i < sizeof(flag); i++)
    {
        if (!wave_put(app, flag[i]))
            return false;
    }

    return true;
}

void txstart(FlipperHamApp *app)
{
    char message[96];
    char dst[CALL_LEN];
    const FlipperHamModemProfile *p;
    const char *path;
    const char *src;
    uint16_t i;
    uint16_t n;
    uint8_t src_ssid;
    uint8_t ssid;
    bool has_ssid;

    app->tx_done = false;
    app->tx_ok = false;
    app->wave_i = 0;
    app->level = true;
    app->wave_len = 0;
    app->wave_carry = 0;
    app->wave_pending = 0;
    app->wave_osc_remain = 0;
    app->wave_prev_h = 0;
    app->pre_b = 0;
    app->pre_h = 0;
    app->pre_c = 0;
    app->pre_a = 0;
    app->pre_us = 0;
    app->pre_k = 0;
    app->wave_is_mark = true;

    if (!app->pkt)
        return;
    if (!app->wave)
        return;
    if (app->tx_msg_index >= TXT_N)
        return;
    p = &flipperham_modem_profiles[1];

    if (app->tx_type == 0)
    {
        if (!app->bulletin_used[app->tx_msg_index])
            return;
        if (!app->bulletin[app->tx_msg_index][0])
            return;
        if (!aprs_bulletin(message, sizeof(message), app->tx_msg_index,
                           app->bulletin[app->tx_msg_index]))
            return;
    }
    else if (app->tx_type == 1)
    {
        if (!app->status_used[app->tx_msg_index])
            return;
        if (!app->status[app->tx_msg_index][0])
            return;
        if (!aprs_status(message, sizeof(message), app->status[app->tx_msg_index]))
            return;
    }
    else if (app->tx_type == 3)
    {
        if (!app->pos_used[app->tx_msg_index])
            return;
        if (!app->pos_name[app->tx_msg_index][0])
            return;
        if (!app->pos_lat[app->tx_msg_index][0])
            return;
        if (!app->pos_lon[app->tx_msg_index][0])
            return;
        if (!aprs_pos(message, sizeof(message), app->pos_name[app->tx_msg_index],
                      app->pos_lat[app->tx_msg_index], app->pos_lon[app->tx_msg_index]))
            return;
    }
    else
    {
        if (app->dst_call_index >= CALL_N)
            return;
        if (!app->message_used[app->tx_msg_index])
            return;
        if (!app->message[app->tx_msg_index][0])
            return;
        if (!app->calls_used[app->dst_call_index])
            return;
        if (!app->calls[app->dst_call_index][0])
            return;

        if (!call_split(app->calls[app->dst_call_index], dst, &ssid, &has_ssid))
            return;
        if (!has_ssid)
            ssid = app->dst_ssid;
        if (!aprs_message(message, sizeof(message), dst, ssid, app->message[app->tx_msg_index]))
            return;
    }

    src = MY_CALL;
    src_ssid = 0;
    path = aprs_path_pick(app);
    if (app->ham_ok)
        if (app->ham_n)
            if (app->ham_index < app->ham_n)
            {
                if (app->ham_calls[app->ham_index][0])
                    src = app->ham_calls[app->ham_index];
                if (app->ham_has_ssid[app->ham_index])
                    src_ssid = app->ham_ssid[app->ham_index];
            }

    if (!aprs_packet(app->pkt, src, src_ssid, MY_TOCALL, 0, message, path))
        return;

    if (app->leadin_ms)
    {
        n = (app->leadin_ms * p->baud + 500) / 1000;
        if (!n)
            n = 1;
        for (i = 0; i < n; i++)
            if (!wave_put(app, 1))
                return;
    }

    if (app->preamble_ms)
    {
        n = (app->preamble_ms * p->baud + 4000) / 8000;
        if (!n)
            n = 1;
        for (i = 0; i < n; i++)
            if (!wave_flag(app))
                return;
    }

    if (!wave_flag(app))
        return;

    for (i = 8; i + 8 < app->pkt->stuffed_len; i++)
    {
        if (!wave_put(app, app->pkt->stuffed[i]))
            return;
    }

    for (i = 0; i < 3; i++)
    {
        if (!wave_flag(app))
            return;
    }

    if (app->wave_pending > (double)0.000001f)
        if (!wave_add(app, app->wave_pending))
            return;
    app->wave_pending = 0;
    app->wave_osc_remain = 0;
    app->wave_prev_h = 0;

    if (!app->wave_len)
        app->tx_done = true;
    else
        app->tx_ok = true;
}
