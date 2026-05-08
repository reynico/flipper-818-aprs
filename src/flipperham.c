#include "flipperham.h"
#include "flipperham_i.h"
#include "app_state.h"
#include "rf_gen.h"

#include <furi_hal_resources.h>
#include <storage/storage.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void cfg_defaults(FlipperHamApp *app)
{
    memset(app->bulletin, 0, sizeof(app->bulletin));
    memset(app->status, 0, sizeof(app->status));
    memset(app->message, 0, sizeof(app->message));
    memset(app->calls, 0, sizeof(app->calls));
    memset(app->pos_name, 0, sizeof(app->pos_name));
    memset(app->pos_lat, 0, sizeof(app->pos_lat));
    memset(app->pos_lon, 0, sizeof(app->pos_lon));

    memset(app->bulletin_used, 0, sizeof(app->bulletin_used));
    memset(app->status_used, 0, sizeof(app->status_used));
    memset(app->message_used, 0, sizeof(app->message_used));
    memset(app->calls_used, 0, sizeof(app->calls_used));
    memset(app->pos_used, 0, sizeof(app->pos_used));

    app->bulletin_n = 0;
    app->status_n = 0;

    app->message_n = 0;
    app->calls_n = 0;
    app->pos_n = 0;

    app->dst_ssid = 0;
    app->repeat_n = 1;
    app->leadin_ms = 50;
    app->preamble_ms = 350;
    app->aprs_path_index = 0;
    app->aprs_path_edit[0] = 0;
    app->debug_tx = false;

    snprintf(app->bulletin[0], sizeof(app->bulletin[0]), "flipper bulletin");
    snprintf(app->status[0], sizeof(app->status[0]), "flipper status");
    snprintf(app->calls[0], sizeof(app->calls[0]), "LU3ARN");
    snprintf(app->calls[1], sizeof(app->calls[1]), "FL1PER");
    snprintf(app->pos_name[0], sizeof(app->pos_name[0]), "Cismigiu Park");
    snprintf(app->pos_lat[0], sizeof(app->pos_lat[0]), "44.437461");
    snprintf(app->pos_lon[0], sizeof(app->pos_lon[0]), "26.090215");
    snprintf(app->pos_name[1], sizeof(app->pos_name[1]), "Null Island");
    snprintf(app->pos_lat[1], sizeof(app->pos_lat[1]), "0.02");
    snprintf(app->pos_lon[1], sizeof(app->pos_lon[1]), "-0.04");

    snprintf(app->message[0], sizeof(app->message[0]), "Hello from Flipper Zero! :D");

    app->bulletin_used[0] = 1;
    app->status_used[0] = 1;
    app->message_used[0] = 1;
    app->calls_used[0] = 1;
    app->calls_used[1] = 1;
    app->pos_used[0] = 1;
    app->pos_used[1] = 1;

    app->bulletin_n = 1;
    app->status_n = 1;
    app->message_n = 1;
    app->calls_n = 2;
    app->pos_n = 2;

    app->dra.ptt_pin = &gpio_ext_pb3;
    app->dra.pd_pin = &gpio_ext_pb2;
    app->dra.sq_pin = &gpio_ext_pc3;
    app->dra_freq = 144.3900f;
    app->dra_freq_index = 0;
    app->dra_volume = 8;
    app->dra_squelch = 4;
    app->has_decoded = false;
    app->rx_active = false;
    app->gps_enabled = false;
    app->beacon_interval = 120;
    app->gps_comment[0] = 0;
}

void cfgsave(FlipperHamApp *app)
{
    Storage *storage;
    File *file;
    FlipperHamCfg *c;

    c = malloc(sizeof(FlipperHamCfg));
    if (!c)
        return;
    memset(c, 0, sizeof(FlipperHamCfg));

    c->dst_ssid = app->dst_ssid;
    c->repeat_n = app->repeat_n;
    c->ham_index = app->ham_index;
    c->leadin_ms = app->leadin_ms;
    c->preamble_ms = app->preamble_ms;
    c->aprs_path_index = app->aprs_path_index;
    c->debug_tx = app->debug_tx;
    c->debug_rx = app->rx_debug;
    c->rx_notify = app->rx_notify;
    c->gps_enabled = app->gps_enabled ? 1 : 0;
    c->beacon_interval = app->beacon_interval;
    memcpy(c->gps_comment, app->gps_comment, sizeof(c->gps_comment));
    c->dra_freq_index = app->dra_freq_index;
    memcpy(c->custom_freq, app->custom_freq_edit, sizeof(c->custom_freq));
    c->dra_volume = app->dra_volume;
    c->dra_squelch = app->dra_squelch;

    memcpy(c->bulletin, app->bulletin, sizeof(c->bulletin));
    memcpy(c->status, app->status, sizeof(c->status));
    memcpy(c->aprs_path_edit, app->aprs_path_edit, sizeof(c->aprs_path_edit));

    memcpy(c->message, app->message, sizeof(c->message));
    memcpy(c->pos_name, app->pos_name, sizeof(c->pos_name));
    memcpy(c->pos_lat, app->pos_lat, sizeof(c->pos_lat));
    memcpy(c->pos_lon, app->pos_lon, sizeof(c->pos_lon));

    memcpy(c->bulletin_used, app->bulletin_used, sizeof(c->bulletin_used));
    memcpy(c->status_used, app->status_used, sizeof(c->status_used));
    memcpy(c->message_used, app->message_used, sizeof(c->message_used));
    memcpy(c->pos_used, app->pos_used, sizeof(c->pos_used));

    c->bulletin_n = app->bulletin_n;
    c->status_n = app->status_n;

    c->message_n = app->message_n;
    c->pos_n = app->pos_n;

    memcpy(c->calls, app->calls, sizeof(c->calls));
    memcpy(c->calls_used, app->calls_used, sizeof(c->calls_used));
    c->calls_n = app->calls_n;

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    storage_common_mkdir(storage, "/ext/apps_data");
    storage_common_mkdir(storage, CFG_DIR);

    if (storage_file_open(file, CFG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(file, c, sizeof(FlipperHamCfg));

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    free(c);
}

void cfgload(FlipperHamApp *app)
{
    Storage *storage;
    File *file;
    FlipperHamCfg *c;
    uint16_t n;
    uint8_t i;

    c = malloc(sizeof(FlipperHamCfg));
    if (!c)
    {
        cfg_defaults(app);
        return;
    }

    memset(c, 0, sizeof(FlipperHamCfg));

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    storage_common_mkdir(storage, "/ext/apps_data");
    storage_common_mkdir(storage, CFG_DIR);

    if (!storage_file_open(file, CFG_FILE, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        free(c);
        cfg_defaults(app);
        cfgsave(app);
        callbook_load_txt(app);
        ham_load_txt(app);
        return;
    }

    n = storage_file_read(file, c, sizeof(FlipperHamCfg));
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if (n != sizeof(FlipperHamCfg))
    {
        free(c);
        cfg_defaults(app);
        cfgsave(app);
        callbook_load_txt(app);
        ham_load_txt(app);
        return;
    }

    app->dst_ssid = c->dst_ssid;
    app->repeat_n = c->repeat_n;
    app->ham_index = c->ham_index;
    app->leadin_ms = c->leadin_ms;
    app->preamble_ms = c->preamble_ms;
    app->aprs_path_index = c->aprs_path_index;
    app->debug_tx = c->debug_tx ? true : false;
    app->rx_debug = c->debug_rx ? true : false;
    app->rx_notify = c->rx_notify ? true : false;
    app->gps_enabled = c->gps_enabled ? true : false;
    app->beacon_interval = c->beacon_interval;
    memcpy(app->gps_comment, c->gps_comment, sizeof(app->gps_comment));
    app->gps_comment[TXT_LEN - 1] = 0;
    app->dra_freq_index = c->dra_freq_index;
    memcpy(app->custom_freq_edit, c->custom_freq, sizeof(app->custom_freq_edit));
    app->custom_freq_edit[sizeof(app->custom_freq_edit) - 1] = 0;
    app->dra_volume = c->dra_volume;
    app->dra_squelch = c->dra_squelch;

    memcpy(app->bulletin, c->bulletin, sizeof(app->bulletin));
    memcpy(app->status, c->status, sizeof(app->status));
    memcpy(app->aprs_path_edit, c->aprs_path_edit, sizeof(app->aprs_path_edit));
    memcpy(app->message, c->message, sizeof(app->message));
    memcpy(app->pos_name, c->pos_name, sizeof(app->pos_name));
    memcpy(app->pos_lat, c->pos_lat, sizeof(app->pos_lat));
    memcpy(app->pos_lon, c->pos_lon, sizeof(app->pos_lon));

    memcpy(app->bulletin_used, c->bulletin_used, sizeof(app->bulletin_used));
    memcpy(app->status_used, c->status_used, sizeof(app->status_used));
    memcpy(app->message_used, c->message_used, sizeof(app->message_used));
    memcpy(app->pos_used, c->pos_used, sizeof(app->pos_used));

    memcpy(app->calls, c->calls, sizeof(app->calls));
    memcpy(app->calls_used, c->calls_used, sizeof(app->calls_used));
    app->calls_n = c->calls_n;

    app->bulletin_n = c->bulletin_n;
    app->status_n = c->status_n;

    app->message_n = c->message_n;
    app->pos_n = c->pos_n;

    if (app->dst_ssid > 15)
        app->dst_ssid = 0;
    if (app->aprs_path_index > 7)
        app->aprs_path_index = 0;
    if (!app->repeat_n || app->repeat_n > 5)
        app->repeat_n = 1;
    if (app->leadin_ms > 1000)
        app->leadin_ms = 1000;
    if (app->preamble_ms > 1000)
        app->preamble_ms = 1000;
    app->leadin_ms = (app->leadin_ms / 50) * 50;
    app->preamble_ms = (app->preamble_ms / 50) * 50;
    if (app->dra_volume < 1 || app->dra_volume > 8)
        app->dra_volume = 8;
    if (app->dra_squelch > 8)
        app->dra_squelch = 4;
    if (app->beacon_interval < 30 || app->beacon_interval > 600)
        app->beacon_interval = 120;

    for (i = 0; i < TXT_N; i++)
    {
        app->bulletin[i][TXT_LEN - 1] = 0;
        app->status[i][TXT_LEN - 1] = 0;
        app->message[i][TXT_LEN - 1] = 0;
        app->pos_name[i][TXT_LEN - 1] = 0;
        app->pos_lat[i][POS_LEN - 1] = 0;
        app->pos_lon[i][POS_LEN - 1] = 0;
    }
    app->aprs_path_edit[APRS_PATH_LEN - 1] = 0;

    bulletin_fix(app);
    status_fix(app);
    message_fix(app);
    position_fix(app);
    callbook_load_txt(app);
    ham_load_txt(app);

    free(c);
}

void bulletin_fix(FlipperHamApp *app)
{
    uint8_t i;

    app->bulletin_n = 0;

    for (i = 0; i < TXT_N; i++)
    {
        if (app->bulletin[i][0])
            app->bulletin_used[i] = 1;
        else
            app->bulletin_used[i] = 0;

        if (app->bulletin_used[i])
            app->bulletin_n++;
    }
}

void status_fix(FlipperHamApp *app)
{
    uint8_t i;

    app->status_n = 0;

    for (i = 0; i < TXT_N; i++)
    {
        if (app->status[i][0])
            app->status_used[i] = 1;
        else
            app->status_used[i] = 0;

        if (app->status_used[i])
            app->status_n++;
    }
}

void calls_fix(FlipperHamApp *app)
{
    uint8_t i;

    app->calls_n = 0;

    for (i = 0; i < CALL_N; i++)
    {
        if (app->calls[i][0])
            app->calls_used[i] = 1;
        else
            app->calls_used[i] = 0;

        if (app->calls_used[i])
            app->calls_n++;
    }
}

void position_fix(FlipperHamApp *app)
{
    uint8_t i;

    app->pos_n = 0;

    for (i = 0; i < TXT_N; i++)
    {
        if (pos_ok(app, i))
            app->pos_used[i] = 1;
        else
            app->pos_used[i] = 0;

        if (app->pos_used[i])
            app->pos_n++;
    }
}

void message_fix(FlipperHamApp *app)
{
    uint8_t i;

    app->message_n = 0;

    for (i = 0; i < TXT_N; i++)
    {
        if (app->message[i][0])
            app->message_used[i] = 1;
        else
            app->message_used[i] = 0;

        if (app->message_used[i])
            app->message_n++;
    }
}

bool call_split(const char *s, char *out, uint8_t *ssid, bool *has_ssid)
{
    char a[CALL_LEN];
    uint8_t i;
    uint8_t j;
    uint8_t k;
    uint8_t n;
    bool dash;

    i = 0;
    j = 0;
    n = 0;
    dash = false;

    while (s[i] && s[i] != '-' && s[i] != '_')
    {
        char c = s[i];

        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
        if (j >= CALL_LEN - 1)
            return false;

        a[j++] = c;
        i++;
    }

    if (s[i] == '_')
        dash = true;
    if (s[i] == '-')
        dash = true;

    if (!dash && j <= 6)
    {
        if (!j)
            return false;
        a[j] = 0;
        snprintf(out, CALL_LEN, "%s", a);
        *has_ssid = false;
        *ssid = 0;
        return true;
    }

    if (!dash)
    {
        k = j;
        while (k && a[k - 1] >= '0' && a[k - 1] <= '9')
            k--;
        if (k == j)
            return false;
        if (k > 6)
            return false;
        if (j - k > 2)
            return false;
        if (!k)
            return false;

        n = 0;
        for (i = k; i < j; i++)
            n = (n * 10) + (a[i] - '0');
        if (n > 15)
            return false;

        a[k] = 0;
        snprintf(out, CALL_LEN, "%s", a);
        *has_ssid = true;
        *ssid = n;
        return true;
    }

    if (j > 6)
        return false;
    if (!j)
        return false;
    i++;
    if (!s[i])
        return false;

    n = 0;
    k = 0;

    while (s[i])
    {
        char c = s[i];

        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c < '0' || c > '9')
            return false;
        n = (n * 10) + (c - '0');
        k++;
        i++;
    }

    if (!k)
        return false;
    if (k > 2)
        return false;
    if (n > 15)
        return false;

    a[j] = 0;
    snprintf(out, CALL_LEN, "%s", a);
    *has_ssid = true;
    *ssid = n;
    return true;
}

bool call_validate(char *s)
{
    char a[CALL_LEN];
    uint8_t b;
    uint8_t i;
    uint8_t j;
    bool d;

    if (!call_split(s, a, &b, &d))
        return false;

    if (d)
    {
        i = 0;
        j = 0;
        while (a[i])
            s[j++] = a[i++];
        s[j++] = '-';
        if (b >= 10)
            s[j++] = '0' + (b / 10);
        s[j++] = '0' + (b % 10);
        s[j] = 0;
    }
    else
        snprintf(s, CALL_LEN, "%s", a);

    return true;
}
