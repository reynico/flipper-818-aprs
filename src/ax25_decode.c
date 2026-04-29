#include "ax25_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t crc_ccitt(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;

    for(uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 1)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFF;
}

static void decode_addr(const uint8_t *raw, char *call, uint8_t *ssid)
{
    uint8_t len = 0;
    for(uint8_t i = 0; i < 6; i++) {
        char c = raw[i] >> 1;
        if(c != ' ')
            call[len++] = c;
    }
    call[len] = 0;
    *ssid = (raw[6] >> 1) & 0x0F;
}

bool ax25_decode_frame(const uint8_t *buf, uint16_t len, AfskFrame *out)
{
    uint16_t pos;
    uint16_t fcs_recv;
    uint16_t fcs_calc;

    memset(out, 0, sizeof(AfskFrame));

    if(len < 17) return false;

    fcs_recv = buf[len - 2] | ((uint16_t)buf[len - 1] << 8);
    fcs_calc = crc_ccitt(buf, len - 2);
    if(fcs_recv != fcs_calc) return false;

    len -= 2;

    decode_addr(buf, out->dst, &out->dst_ssid);
    decode_addr(buf + 7, out->src, &out->src_ssid);
    pos = 14;

    uint8_t path_pos = 0;
    bool last = buf[13] & 1;

    while(!last && pos + 7 <= len) {
        char digi[10];
        uint8_t digi_ssid;
        decode_addr(buf + pos, digi, &digi_ssid);
        last = buf[pos + 6] & 1;
        pos += 7;

        if(path_pos && path_pos < sizeof(out->path) - 1)
            out->path[path_pos++] = ',';

        uint8_t dl = strlen(digi);
        for(uint8_t i = 0; i < dl && path_pos < sizeof(out->path) - 4; i++)
            out->path[path_pos++] = digi[i];

        if(digi_ssid) {
            out->path[path_pos++] = '-';
            if(digi_ssid >= 10)
                out->path[path_pos++] = '0' + digi_ssid / 10;
            out->path[path_pos++] = '0' + digi_ssid % 10;
        }
    }
    out->path[path_pos] = 0;

    if(pos + 2 > len) return false;
    pos += 2;

    out->payload_len = len - pos;
    if(out->payload_len > sizeof(out->payload))
        out->payload_len = sizeof(out->payload);
    memcpy(out->payload, buf + pos, out->payload_len);

    out->valid = true;
    return true;
}

/* ── APRS position parsing ──────────────────────────────────────── */

static bool parse_position(const char *s, uint16_t len, AprsDecoded *out)
{
    int d;
    float m;

    if(len < 19) return false;

    if(s[0] < '0' || s[0] > '9') return false;
    if(s[1] < '0' || s[1] > '9') return false;

    d = (s[0] - '0') * 10 + (s[1] - '0');
    m = (float)((s[2] - '0') * 10 + (s[3] - '0'));
    if(s[4] == '.')
        m += (float)(s[5] - '0') * 0.1f + (float)(s[6] - '0') * 0.01f;
    out->lat = (float)d + m / 60.0f;
    if(s[7] == 'S' || s[7] == 's') out->lat = -out->lat;

    out->symbol[0] = s[8];

    if(s[9] < '0' || s[9] > '9') return false;

    d = (s[9] - '0') * 100 + (s[10] - '0') * 10 + (s[11] - '0');
    m = (float)((s[12] - '0') * 10 + (s[13] - '0'));
    if(s[14] == '.')
        m += (float)(s[15] - '0') * 0.1f + (float)(s[16] - '0') * 0.01f;
    out->lon = (float)d + m / 60.0f;
    if(s[17] == 'W' || s[17] == 'w') out->lon = -out->lon;

    out->symbol[1] = s[18];
    out->has_pos = true;

    if(len > 19)
        snprintf(out->comment, sizeof(out->comment), "%.*s", (int)(len - 19), s + 19);

    return true;
}

static bool parse_message(const char *s, uint16_t len, AprsDecoded *out)
{
    uint8_t i = 0;
    uint8_t t = 0;
    uint8_t ml = 0;

    if(len < 10) return false;

    while(i < 9 && i < len && s[i] != ':') {
        if(s[i] != ' ')
            out->msg_to[t++] = s[i];
        i++;
    }
    out->msg_to[t] = 0;

    while(i < len && s[i] != ':') i++;
    if(i >= len) return false;
    i++;

    while(i < len && s[i] != '{' && ml < sizeof(out->msg_text) - 1)
        out->msg_text[ml++] = s[i++];
    out->msg_text[ml] = 0;
    out->has_msg = true;

    return true;
}

bool aprs_decode(const AfskFrame *frame, AprsDecoded *out)
{
    const char *data;
    uint16_t len;

    memset(out, 0, sizeof(AprsDecoded));

    snprintf(out->src, sizeof(out->src), "%s", frame->src);
    if(frame->src_ssid) {
        size_t sl = strlen(out->src);
        snprintf(out->src + sl, sizeof(out->src) - sl, "-%u", frame->src_ssid);
    }

    snprintf(out->dst, sizeof(out->dst), "%s", frame->dst);
    snprintf(out->path, sizeof(out->path), "%s", frame->path);

    if(!frame->payload_len) return false;

    data = (const char *)frame->payload;
    len = frame->payload_len;
    out->type = data[0];

    switch(out->type) {
    case '!':
    case '=':
        return parse_position(data + 1, len - 1, out);
    case '/':
    case '@':
        if(len < 8) return false;
        return parse_position(data + 8, len - 8, out);
    case ':':
        return parse_message(data + 1, len - 1, out);
    case '>':
        if(len > 1)
            snprintf(out->comment, sizeof(out->comment), "%.*s", (int)(len - 1), data + 1);
        return true;
    default:
        snprintf(out->comment, sizeof(out->comment), "%.*s", (int)len, data);
        return true;
    }
}
