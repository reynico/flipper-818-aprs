#include "gps.h"

#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>

static const char *nmea_field(const char *s, uint8_t n)
{
    if (!s) return NULL;
    while (n--)
    {
        s = strchr(s, ',');
        if (!s) return NULL;
        s++;
    }
    return s;
}

static float nmea_field_float(const char *s, uint8_t n)
{
    const char *f = nmea_field(s, n);
    if (!f || !*f || *f == ',') return 0.0f;
    return strtof(f, NULL);
}

static char nmea_field_char(const char *s, uint8_t n)
{
    const char *f = nmea_field(s, n);
    if (!f || !*f || *f == ',') return 0;
    return *f;
}

static float nmea_coord(float raw, char hemi)
{
    int deg = (int)(raw / 100.0f);
    float min = raw - deg * 100.0f;
    float result = deg + min / 60.0f;
    if (hemi == 'S' || hemi == 'W') result = -result;
    return result;
}

static bool nmea_checksum(const char *s)
{
    uint8_t ck = 0;
    const char *p;

    if (!s || *s != '$') return false;
    p = s + 1;
    while (*p && *p != '*') ck ^= (uint8_t)*p++;
    if (*p != '*') return false;
    p++;
    if (!p[0] || !p[1]) return false;

    char h[3] = {p[0], p[1], 0};
    uint8_t expected = (uint8_t)strtoul(h, NULL, 16);
    return ck == expected;
}

static bool nmea_match(const char *s, const char *suffix)
{
    if (!s || s[0] != '$') return false;
    size_t slen = strlen(suffix);
    if (s[1] == 'G' && s[2] != 0)
        return (strncmp(s + 3, suffix, slen) == 0);
    return false;
}

static void parse_rmc(Gps *gps, const char *s)
{
    char status;
    float raw_lat, raw_lon;
    char ns, ew;

    status = nmea_field_char(s, 2);
    if (status != 'A')
    {
        gps->valid = false;
        return;
    }

    raw_lat = nmea_field_float(s, 3);
    ns = nmea_field_char(s, 4);
    raw_lon = nmea_field_float(s, 5);
    ew = nmea_field_char(s, 6);

    if (!ns || !ew) return;

    gps->lat = nmea_coord(raw_lat, ns);
    gps->lon = nmea_coord(raw_lon, ew);
    gps->speed_knots = nmea_field_float(s, 7);
    gps->course = nmea_field_float(s, 8);
    gps->valid = true;
    gps->last_fix_tick = furi_get_tick();
}

static void parse_gga(Gps *gps, const char *s)
{
    gps->fix_quality = (uint8_t)nmea_field_float(s, 6);
    gps->sats = (uint8_t)nmea_field_float(s, 7);
    gps->altitude = nmea_field_float(s, 9);
}

static void nmea_parse(Gps *gps)
{
    const char *s = gps->nmea_buf;

    if (!nmea_checksum(s)) return;

    if (nmea_match(s, "RMC"))
        parse_rmc(gps, s);
    else if (nmea_match(s, "GGA"))
        parse_gga(gps, s);
}

static int32_t gps_worker(void *ctx)
{
    Gps *gps = ctx;
    uint8_t c;

    while (gps->running)
    {
        size_t len = furi_stream_buffer_receive(gps->rx_buf, &c, 1, 100);
        if (!len) continue;

        if (c == '\n')
        {
            gps->nmea_buf[gps->nmea_len] = 0;
            if (gps->nmea_len > 6)
                nmea_parse(gps);
            gps->nmea_len = 0;
        }
        else if (c != '\r' && gps->nmea_len < sizeof(gps->nmea_buf) - 1)
        {
            gps->nmea_buf[gps->nmea_len++] = c;
        }
    }

    return 0;
}

static void gps_rx_cb(
    FuriHalSerialHandle *handle,
    FuriHalSerialRxEvent event,
    void *ctx)
{
    Gps *gps = ctx;

    if (event == FuriHalSerialRxEventData)
    {
        uint8_t c = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(gps->rx_buf, &c, 1, 0);
        gps->rx_count++;
    }
}

void gps_start(Gps *gps)
{
    if (!gps) return;
    if (gps->running) return;

    memset(gps->nmea_buf, 0, sizeof(gps->nmea_buf));
    gps->nmea_len = 0;
    gps->valid = false;
    gps->sats = 0;
    gps->fix_quality = 0;
    gps->lat = 0;
    gps->lon = 0;
    gps->speed_knots = 0;
    gps->course = 0;
    gps->altitude = 0;
    gps->last_fix_tick = 0;
    gps->rx_count = 0;
    gps->start_tick = furi_get_tick();

    gps->rx_buf = furi_stream_buffer_alloc(256, 1);
    gps->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if (!gps->serial)
    {
        furi_stream_buffer_free(gps->rx_buf);
        gps->rx_buf = NULL;
        return;
    }

    furi_hal_serial_init(gps->serial, 9600);
    gps->running = true;

    gps->worker = furi_thread_alloc_ex("gps_worker", 1024, gps_worker, gps);
    furi_thread_start(gps->worker);

    furi_hal_serial_async_rx_start(gps->serial, gps_rx_cb, gps, false);
}

void gps_stop(Gps *gps)
{
    if (!gps) return;
    if (!gps->running) return;

    gps->running = false;

    if (gps->serial)
    {
        furi_hal_serial_async_rx_stop(gps->serial);
        furi_hal_serial_deinit(gps->serial);
        furi_hal_serial_control_release(gps->serial);
        gps->serial = NULL;
    }

    if (gps->worker)
    {
        furi_thread_join(gps->worker);
        furi_thread_free(gps->worker);
        gps->worker = NULL;
    }

    if (gps->rx_buf)
    {
        furi_stream_buffer_free(gps->rx_buf);
        gps->rx_buf = NULL;
    }

    gps->valid = false;
}
