#include "hot/time.h"

#include <limits.h>
#include <time.h>

hot_time_t hot_now_word(void)
{
    time_t now = time(NULL);
    if (now < (time_t)0)
        return (hot_time_t)0;
    return (hot_time_t)now;
}

hot_time_t hot_from_64(int64_t seconds)
{
    if (seconds < 0)
        return (hot_time_t)0;
    return (hot_time_t)seconds;
}

hot_time_t hot_from_32(int32_t legacy, uint32_t era)
{
    return HOT_BIT_32COMPAT
         | ((uint64_t)(era & HOT_ERA_MASK) << HOT_ERA_SHIFT)
         | (uint32_t)legacy;
}

hot_time_t hot_from_u32(uint32_t legacy, uint32_t era)
{
    uint64_t t = ((uint64_t)era << 32) | legacy;
    uint64_t e = (t >> 32)
               + (uint64_t)((t & UINT64_C(0xFFFFFFFF)) >= UINT64_C(0x80000000));
    if (e > HOT_ERA_MAX)
        e = HOT_ERA_MAX;
    return HOT_BIT_32COMPAT | (e << HOT_ERA_SHIFT) | legacy;
}

int64_t hot_seconds(const hot_time_t word)
{
    if ((word & HOT_BIT_32COMPAT) == 0)
        return (int64_t)(word & HOT_VALUE_MASK);
    uint32_t era = (uint32_t)((word >> HOT_ERA_SHIFT) & HOT_ERA_MASK);
    int32_t legacy = (int32_t)(uint32_t)(word & UINT64_C(0xFFFFFFFF));
    return (int64_t)era * INT64_C(4294967296) + legacy;
}

bool hot_is_compat(const hot_time_t word)
{
    return (word & HOT_BIT_32COMPAT) != 0;
}

uint32_t hot_era_of(const hot_time_t word)
{
    if ((word & HOT_BIT_32COMPAT) != 0)
        return (uint32_t)((word >> HOT_ERA_SHIFT) & HOT_ERA_MASK);
    uint64_t v = word & HOT_VALUE_MASK;
    uint64_t era = (v >> 32)
                 + (uint64_t)((v & UINT64_C(0xFFFFFFFF)) >= UINT64_C(0x80000000));
    return (uint32_t)era;
}

int hot_compat_active(void)
{
    if (sizeof(time_t) * CHAR_BIT >= 64)
        return 0;
    time_t now = time(NULL);
    if (now == (time_t)-1)
        return 0;
    return (uintmax_t)now >= UINTMAX_C(0x7FFFFFFF) ? 1 : 0;
}

static int64_t era_instant(uint32_t era, int32_t legacy)
{
    return (int64_t)((uint64_t)era << 32) + (int64_t)legacy;
}

int hot_from_32_bounded(int32_t legacy, int64_t lower, int64_t upper,
                        hot_time_t *out)
{
    uint64_t n = (uint64_t)HOT_ERA_MAX + 1;
    uint64_t lo = 0, len = n;
    while (len > 0) {
        uint64_t half = len >> 1;
        uint64_t mid = lo + half;
        if (era_instant((uint32_t)mid, legacy) < lower) {
            lo = mid + 1;
            len -= half + 1;
        } else {
            len = half;
        }
    }
    uint64_t p = 0;
    len = n;
    while (len > 0) {
        uint64_t half = len >> 1;
        uint64_t mid = p + half;
        if (era_instant((uint32_t)mid, legacy) <= upper) {
            p = mid + 1;
            len -= half + 1;
        } else {
            len = half;
        }
    }
    if (lo >= p || p - lo > 1)
        return HOT_EAMBIGUOUS;
    if (out != NULL)
        *out = hot_from_32(legacy, (uint32_t)lo);
    return 0;
}

int hot_to_32(int64_t seconds, enum hot_wrap_policy policy, int32_t *out)
{
    switch (policy) {
    case HOT_WRAP:
        if (out != NULL)
            *out = (int32_t)(uint32_t)(uint64_t)seconds; /* hot-cast-gate: allow */
        return 0;
    case HOT_SATURATE:
        if (out != NULL) {
            if (seconds < INT32_MIN)
                *out = INT32_MIN;
            else if (seconds > INT32_MAX)
                *out = INT32_MAX;
            else
                *out = (int32_t)seconds; /* hot-cast-gate: allow */
        }
        return 0;
    default:
        break;
    }
    if (seconds < INT32_MIN || seconds > INT32_MAX)
        return HOT_ERANGE;
    if (out != NULL)
        *out = (int32_t)seconds; /* hot-cast-gate: allow */
    return 0;
}