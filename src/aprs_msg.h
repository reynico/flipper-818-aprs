#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APRS_MSGNO_LEN 6
#define ACK_CACHE_N 8
#define ACK_KEY_S 30u
#define ACK_GLOBAL_S 5u

typedef struct
{
    char to[10];
    char text[68];
    char no[APRS_MSGNO_LEN];
    bool is_ack;
    bool is_rej;
} AprsMsg;

typedef struct
{
    char src[10];
    char no[APRS_MSGNO_LEN];
    uint32_t t;
    bool used;
} AckEntry;

typedef struct
{
    AckEntry e[ACK_CACHE_N];
    uint32_t last_s;
    bool any;
    uint8_t next;
} AckCache;

bool aprs_msgno_ok(const char *s);
bool aprs_msg_parse(const char *s, uint16_t len, AprsMsg *out);
void ack_cache_reset(AckCache *c);
bool ack_allow(AckCache *c, const char *src, const char *no, uint32_t now_s);
