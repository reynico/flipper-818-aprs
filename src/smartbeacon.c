#include "smartbeacon.h"

void sb_reset(SmartBeacon *sb)
{
    sb->has_last = false;
    sb->last_s = 0;
    sb->last_course = 0;
}

uint32_t sb_rate(float speed_kmh)
{
    float r;

    if (speed_kmh <= SB_LOW_SPEED_KMH)
        return SB_SLOW_RATE_S;
    if (speed_kmh >= SB_HIGH_SPEED_KMH)
        return SB_FAST_RATE_S;

    r = (float)SB_FAST_RATE_S * SB_HIGH_SPEED_KMH / speed_kmh;
    if (r > (float)SB_SLOW_RATE_S)
        return SB_SLOW_RATE_S;
    return (uint32_t)r;
}

static float sb_heading_delta(float a, float b)
{
    float d = a - b;

    while (d < 0.0f)
        d += 360.0f;
    while (d >= 360.0f)
        d -= 360.0f;
    if (d > 180.0f)
        d = 360.0f - d;
    return d;
}

bool sb_due(const SmartBeacon *sb, uint32_t now_s, float speed_kmh, float course)
{
    uint32_t elapsed;

    if (!sb->has_last)
        return true;

    elapsed = now_s - sb->last_s;
    if (elapsed >= sb_rate(speed_kmh))
        return true;

    if (speed_kmh <= SB_LOW_SPEED_KMH)
        return false;
    if (elapsed < SB_TURN_TIME_S)
        return false;

    return sb_heading_delta(course, sb->last_course) >
           SB_TURN_MIN_DEG + SB_TURN_SLOPE / speed_kmh;
}

void sb_sent(SmartBeacon *sb, uint32_t now_s, float course)
{
    sb->has_last = true;
    sb->last_s = now_s;
    sb->last_course = course;
}
