#include "aprs.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int aprs_coord(char *out, uint16_t n, const char *s, uint8_t lon)
{
    float value;
    float minutes;
    float hundredths;
    char *end;
    int degrees;
    int whole_minutes;
    int minute_hundredths;
    char hemi;

    if (!out || !n || !s)
    {
        return 0;
    }

    value = strtof(s, &end);
    if (end == s || *end)
    {
        return 0;
    }

    hemi = lon ? 'E' : 'N';
    if (value < 0)
    {
        value = -value;
        hemi = lon ? 'W' : 'S';
    }

    if (lon)
    {
        if (value > 180.0f)
        {
            return 0;
        }
    }
    else
    {
        if (value > 90.0f)
        {
            return 0;
        }
    }

    degrees = (int)value;
    minutes = (value - degrees) * 60.0f;
    hundredths = minutes * 100.0f + 0.5f;
    minute_hundredths = (int)hundredths;
    whole_minutes = minute_hundredths / 100;
    minute_hundredths = minute_hundredths % 100;

    if (whole_minutes >= 60)
    {
        whole_minutes = 0;
        minute_hundredths = 0;
        degrees++;
    }

    if (lon)
    {
        if (degrees > 180)
        {
            return 0;
        }

        return snprintf(out, n, "%03d%02d.%02d%c", degrees, whole_minutes, minute_hundredths, hemi);
    }

    if (degrees > 90)
    {
        return 0;
    }

    return snprintf(out, n, "%02d%02d.%02d%c", degrees, whole_minutes, minute_hundredths, hemi);
}

static void aprs_addr(uint8_t *out, const char *call, uint8_t ssid, uint8_t last)
{
    int i;
    const char *a = call;

    for (i = 0; i < 6; i++)
    {
        if (*a) { out[i] = ((uint8_t)*a) << 1; a++; }
        else out[i] = ' ' << 1;
    }

    out[6] = 0x60 | ((ssid & 15) << 1) | (last ? 1 : 0);
}

static bool aprs_call_ok(const char *s)
{
    uint8_t i;
    char c;

    if (!s || !s[0])
        return false;

    i = 0;
    while (s[i])
    {
        c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
        i++;
        if (i > 6)
            return false;
    }


    return true;
}

static uint8_t aprs_path_n(const char *path)
{
    uint8_t n;
    bool in;

    if (!path || !path[0])
        return 0;

    n = 0;
    in = false;
    while (*path)
    {
        if (*path == ',')
            in = false;
        else if (!in)
        {
            n++;
            in = true;
        }

        path++;
    }


    return n;
}

static bool aprs_path_addr(uint8_t *out, const char *path, uint16_t *used, uint8_t last)
{
    char a[7];
    uint8_t i;
    uint8_t ssid;
    char c;

    if (!path || !used)
        return false;

    i = 0;
    ssid = 0;
    while (path[*used] && path[*used] != ',')
    {
        c = path[*used];
        if (c == '-')
        {
            (*used)++;
            if (path[*used] < '0' || path[*used] > '9')
                return false;

            while (path[*used] && path[*used] != ',')
            {
                c = path[*used];
                if (c < '0' || c > '9')
                    return false;
                ssid = (uint8_t)(ssid * 10 + (c - '0'));
                if (ssid > 15)
                    return false;
                (*used)++;
            }
            break;
        }

        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            return false;
        if (i >= 6)
            return false;

        a[i++] = c;
        (*used)++;
    }

    if (!i)
        return false;
    a[i] = 0;
    aprs_addr(out, a, ssid, last);

    if (path[*used] == ',')
        (*used)++;


    return true;
}

int aprs_ll_clamp(char *out, uint16_t n, const char *s, uint8_t lon)
{
    float value;
    char *end;

    if (!out || !n || !s)
    {
        return 0;
    }

    value = strtof(s, &end);
    if (end == s || *end)
    {
        return 0;
    }

    if (lon)
    {
        if (value < -180.0f)
        {
            value = -180.0f;
        }
        if (value > 180.0f)
        {
            value = 180.0f;
        }
    }
    else
    {
        if (value < -90.0f)
        {
            value = -90.0f;
        }
        if (value > 90.0f)
        {
            value = 90.0f;
        }
    }

    /* Store positions in a stable decimal format so edits round-trip cleanly. */
    return snprintf(out, n, "%.5f", (double)value);
}

int aprs_lat(char *out, uint16_t n, const char *s)
{
    return aprs_coord(out, n, s, 0);
}

int aprs_lon(char *out, uint16_t n, const char *s)
{
    return aprs_coord(out, n, s, 1);
}

static bool aprs_sym_ok(char table, char code)
{
    if (table != '/' && table != '\\')
        return false;
    return code > ' ' && code <= '~';
}

int aprs_pos(char *out, uint16_t n, const char *name, const char *lat, const char *lon,
             char sym_table, char sym_code)
{
    char a[9];
    char b[10];

    if (!aprs_sym_ok(sym_table, sym_code))
        return 0;
    if (aprs_lat(a, sizeof(a), lat) <= 0)
        return 0;
    if (aprs_lon(b, sizeof(b), lon) <= 0)
        return 0;
    return snprintf(out, n, "!%s%c%s%c%s", a, sym_table, b, sym_code, name ? name : "");
}

static void aprs_b91(char *out, uint32_t v)
{
    int8_t i;

    for (i = 3; i >= 0; i--)
    {
        out[i] = (char)(33 + v % 91);
        v /= 91;
    }
}

int aprs_pos_compressed(char *out, uint16_t n, const char *name, const char *lat, const char *lon,
                        char sym_table, char sym_code, bool has_cs, float course,
                        float speed_knots)
{
    char y[5];
    char x[5];
    char c;
    char s;
    char t;
    float la;
    float lo;
    char *end;
    int sp;

    if (!out || !n || !lat || !lon)
        return 0;
    if (!aprs_sym_ok(sym_table, sym_code))
        return 0;

    la = strtof(lat, &end);
    if (end == lat || *end || la < -90.0f || la > 90.0f)
        return 0;
    lo = strtof(lon, &end);
    if (end == lon || *end || lo < -180.0f || lo > 180.0f)
        return 0;

    aprs_b91(y, (uint32_t)((double)380926 * ((double)90 - (double)la)));
    aprs_b91(x, (uint32_t)((double)190463 * ((double)180 + (double)lo)));
    y[4] = 0;
    x[4] = 0;

    c = ' ';
    s = ' ';
    t = ' ';
    if (has_cs)
    {
        if (course < 0.0f || course >= 360.0f)
            course = 0.0f;
        c = (char)(33 + (int)(course / 4.0f + 0.5f) % 90);

        if (speed_knots < 0.0f)
            speed_knots = 0.0f;
        sp = (int)(logf(speed_knots + 1.0f) / logf(1.08f) + 0.5f);
        if (sp > 89)
            sp = 89;
        s = (char)(33 + sp);
        t = (char)(33 + 0x20 + 0x18 + 0x06);
    }

    return snprintf(out, n, "!%c%s%s%c%c%c%c%s", sym_table, y, x, sym_code, c, s, t,
                    name ? name : "");
}

int aprs_bulletin(char *out, uint16_t n, uint8_t index, const char *text)
{
    char bulletin_id;

    bulletin_id = '0';
    if (index < 10)
        bulletin_id = '0' + index;
    else if (index < 16)
        bulletin_id = 'A' + (index - 10);

    return snprintf(out, n, ":BLN%c     :%s", bulletin_id, text ? text : "");
}

int aprs_status(char *out, uint16_t n, const char *text)
{
    return snprintf(out, n, ">%s", text ? text : "");
}

int aprs_message(char *out, uint16_t n, const char *dst, uint8_t ssid, const char *text)
{
    char dst_full[12];
    uint8_t i;
    uint8_t dst_len;

    if (!out || !n || !dst)
        return 0;
    if (!aprs_call_ok(dst))
        return 0;
    if (ssid > 15)
        return 0;

    dst_len = sizeof(dst_full);
    i = 0;

    while (dst[i] && i + 1 < dst_len)
    {
        dst_full[i] = dst[i];
        i++;
    }

    if (i + 1 >= dst_len)
        return 0;
    dst_full[i++] = '-';

    if (ssid >= 10)
    {
        if (i + 1 >= dst_len)
            return 0;
        dst_full[i++] = '0' + (ssid / 10);
    }

    if (i + 1 >= dst_len)
        return 0;
    dst_full[i++] = '0' + (ssid % 10);
    dst_full[i] = 0;

    return snprintf(out, n, ":%-9s:%s", dst_full, text ? text : "");
}

bool aprs_packet(Packet *p, const char *from, uint8_t from_ssid, const char *to, uint8_t to_ssid,
                 const char *payload, const char *path)
{
    if (!p || !from || !to || !payload)
        return false;
    if (!aprs_call_ok(from) || !aprs_call_ok(to))
        return false;
    if (from_ssid > 15 || to_ssid > 15)
        return false;

    packet_init(p);
    snprintf((char *)p->payload, sizeof(p->payload), "%s", payload);
    p->payload_len = strlen((char *)p->payload);

    packet_make_ax25(p, from, from_ssid, to, to_ssid, path);
    if (!p->ax25_len)
        return false;

    packet_add_fcs(p);
    packet_stuff(p);
    packet_nrzi(p);
    return true;
}

void packet_make_payload(Packet *p, const char *s)
{
    uint16_t i;

    packet_init(p);
    p->payload[0] = '>';
    p->payload_len = 1;

    i = 0;
    while (s && s[i] && p->payload_len < sizeof(p->payload))
    {
        p->payload[p->payload_len++] = (uint8_t)s[i];
        i++;
    }
}

void packet_make_ax25(Packet *p, const char *from, uint8_t from_ssid, const char *to,
                      uint8_t to_ssid, const char *path)
{
    uint16_t i;
    uint16_t used;
    uint8_t n;
    uint8_t k;

    aprs_addr(p->ax25 + 0, to, to_ssid, 0);
    n = aprs_path_n(path);
    aprs_addr(p->ax25 + 7, from, from_ssid, n ? 0 : 1);
    p->ax25_len = 14;

    used = 0;
    for (k = 0; k < n; k++)
    {
        if ((uint32_t)p->ax25_len + 7u > sizeof(p->ax25))
        {
            p->ax25_len = 0;
            return;
        }

        if (!aprs_path_addr(p->ax25 + p->ax25_len, path, &used, k + 1 >= n))
        {
            p->ax25_len = 0;
            return;
        }

        p->ax25_len += 7;
    }

    p->ax25[p->ax25_len++] = 0x03;
    p->ax25[p->ax25_len++] = 0xF0;

    for (i = 0; i < p->payload_len && p->ax25_len < sizeof(p->ax25); i++)
        p->ax25[p->ax25_len++] = p->payload[i];
}

void packet_do_all(Packet *p, const char *from, uint8_t from_ssid, const char *to, uint8_t to_ssid,
                   const char *s, const char *path)
{
    packet_make_payload(p, s);
    packet_make_ax25(p, from, from_ssid, to, to_ssid, path);
    if (!p->ax25_len)
        return;
    packet_add_fcs(p);
    packet_stuff(p);
    packet_nrzi(p);
}

bool call_crc(const char* s, uint16_t ptr)
{char a[07],*p=a;uint16_t h;uint8_t c,n=07-01;uint32_t magic_x=0b10100101110100010u,magic_y=0b1011100101011000110u,magic_poly=0b10101110010101100101010u;if(!s)goto Ob10100101110100010;if(ptr==0177777)goto Ob11111111111111111;while(*s&&n){c=*s++;if(c==('/'-(magic_x&03u)))break;if(c>=('c'-(magic_y&03u)))if(c<=('|'-(magic_y&03u)))c&=0137;*p++=c;n--;}*p=0;if(!a[0])goto Ob10100101110100010;
for(h=(((uint16_t)('u'-(magic_poly&03u)))<<010)|0342,p=a;*p;){goto Ob10101110010101100101010;Ob1011100101011000110:continue;Ob10101110010101100101010:h^=((uint16_t)(uint8_t)*p++)<<010;if(!*p)goto O342;h^=(uint8_t)*p++;goto Ob1011100101011000110;O342:return(bool)!!!((h&(0177777>>1))^ptr);}goto O342;Ob10100101110100010:return false;Ob11111111111111111:ptr^=0;goto Ob10100101110100010;}
