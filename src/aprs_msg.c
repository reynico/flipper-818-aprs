#include "aprs_msg.h"

#include <string.h>

static bool msgno_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool aprs_msgno_ok(const char *s)
{
    uint8_t i;

    if (!s)
        return false;
    for (i = 0; s[i]; i++)
        if (i >= APRS_MSGNO_LEN - 1 || !msgno_char(s[i]))
            return false;
    return i > 0;
}

static bool msgno_take(const char *s, uint16_t len, char *out)
{
    uint16_t i;

    for (i = 0; i < len && s[i] != '}'; i++)
    {
        if (i >= APRS_MSGNO_LEN - 1 || !msgno_char(s[i]))
            return false;
        out[i] = s[i];
    }
    out[i] = 0;
    return i > 0;
}

bool aprs_msg_parse(const char *s, uint16_t len, AprsMsg *out)
{
    uint16_t i = 0;
    uint16_t t = 0;
    uint16_t text_len;
    uint16_t brace;
    const char *text;

    memset(out, 0, sizeof(AprsMsg));
    if (!s || len < 10)
        return false;

    while (i < 9 && i < len && s[i] != ':')
    {
        if (s[i] != ' ')
            out->to[t++] = s[i];
        i++;
    }
    out->to[t] = 0;

    while (i < len && s[i] != ':')
        i++;
    if (i >= len)
        return false;
    i++;

    text = s + i;
    text_len = len - i;

    if (text_len > 3 && (!strncmp(text, "ack", 3) || !strncmp(text, "rej", 3)))
    {
        if (msgno_take(text + 3, text_len - 3, out->no))
        {
            out->is_ack = text[0] == 'a';
            out->is_rej = text[0] == 'r';
        }
    }

    brace = text_len;
    for (i = 0; i < text_len; i++)
        if (text[i] == '{')
        {
            brace = i;
            break;
        }

    if (!out->is_ack && !out->is_rej && brace < text_len)
        if (!msgno_take(text + brace + 1, text_len - brace - 1, out->no))
            out->no[0] = 0;

    for (i = 0; i < brace && i < sizeof(out->text) - 1; i++)
        out->text[i] = text[i];
    out->text[i] = 0;

    return true;
}

void ack_cache_reset(AckCache *c)
{
    memset(c, 0, sizeof(AckCache));
}

bool ack_allow(AckCache *c, const char *src, const char *no, uint32_t now_s)
{
    uint8_t i;
    AckEntry *e;

    if (!c || !src || !no)
        return false;
    if (c->any && now_s - c->last_s < ACK_GLOBAL_S)
        return false;

    for (i = 0; i < ACK_CACHE_N; i++)
    {
        e = &c->e[i];
        if (e->used && !strcmp(e->src, src) && !strcmp(e->no, no))
        {
            if (now_s - e->t < ACK_KEY_S)
                return false;
            e->t = now_s;
            c->last_s = now_s;
            c->any = true;
            return true;
        }
    }

    e = &c->e[c->next];
    c->next = (uint8_t)((c->next + 1) % ACK_CACHE_N);
    strncpy(e->src, src, sizeof(e->src) - 1);
    e->src[sizeof(e->src) - 1] = 0;
    strncpy(e->no, no, sizeof(e->no) - 1);
    e->no[sizeof(e->no) - 1] = 0;
    e->t = now_s;
    e->used = true;
    c->last_s = now_s;
    c->any = true;
    return true;
}
