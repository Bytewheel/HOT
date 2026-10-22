#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hot/time.h"

static uint64_t rng = UINT64_C(0x7BEEF5EEDC0FEE5E);

static uint64_t rnd64(void)
{
    uint64_t x = rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng = x;
}

static uint64_t bad;
static char firstbad[96];

static void vio(const char *what, uint64_t sample)
{
    if (bad == 0)
        snprintf(firstbad, sizeof firstbad, "%s (sample 0x%016" PRIX64 ")",
                 what, sample);
    bad++;
}

static const hot_time_t BND[] = {
    0, 1, 2,
    UINT64_C(0x7FFFFFFF), UINT64_C(0x80000000), UINT64_C(0xFFFFFFFF),
    UINT64_C(0x100000000),
    UINT64_C(0x7FFFFFFFFFFFFFFE), UINT64_C(0x7FFFFFFFFFFFFFFF),
    UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000001),
    UINT64_C(0x800000007FFFFFFF), UINT64_C(0x8000000180000000),
    UINT64_C(0xFFFFFFFFFFFFFFFF),
};

static const int64_t S64[] = {
    INT64_MIN, INT64_MIN + 1, INT64_C(-2147483649), INT64_C(-2147483648),
    INT64_C(-1), 0, 1, INT64_C(2147483647), INT64_C(2147483648),
    INT64_C(4294967295), INT64_C(4294967296), INT64_MAX - 1, INT64_MAX,
};

static int32_t u32_to_i32(uint32_t u)
{
    int32_t r;
    memcpy(&r, &u, sizeof r);
    return r;
}

static int64_t ref_seconds(hot_time_t w)
{
    if ((w & HOT_BIT_32COMPAT) == 0)
        return (int64_t)(w & HOT_VALUE_MASK);
    uint64_t low = w & UINT64_C(0xFFFFFFFF);
    int64_t lowi = (low >= UINT64_C(0x80000000))
                       ? (int64_t)(low - UINT64_C(0x80000000))
                             - INT64_C(2147483648)
                       : (int64_t)low;
    uint64_t era = (w >> HOT_ERA_SHIFT) & HOT_ERA_MASK;
    return (int64_t)era * INT64_C(4294967296) + lowi;
}

static void vet_word(hot_time_t w)
{
    int cls = (int)(w >> 63);

    if (hot_is_compat(w) != (cls != 0))
        vio("is_compat classification", w);
    if (hot_seconds(w) != ref_seconds(w))
        vio("seconds vs independent reference", w);

    if (cls) {
        int32_t low32 = u32_to_i32((uint32_t)w);
        uint32_t era = (uint32_t)((w >> HOT_ERA_SHIFT) & HOT_ERA_MASK);
        if (hot_era_of(w) != era)
            vio("era_of tagged", w);
        if (hot_from_32(low32, era) != w)
            vio("from_32 round-trip", w);
        if (hot_from_32(low32, era ^ UINT32_C(0x80000000)) != w)
            vio("from_32 era bit 31 not masked", w);
    } else {
        int64_t s = (int64_t)(w & HOT_VALUE_MASK);
        uint64_t era = ((uint64_t)s + UINT64_C(2147483648)) >> 32;
        if ((uint64_t)hot_era_of(w) != era)
            vio("era_of untagged", w);
        if (s >= 0 && hot_from_64(s) != w)
            vio("from_64 round-trip", w);
    }
}

static void vet_from64(int64_t s)
{
    hot_time_t want = (s < 0) ? (hot_time_t)0 : (hot_time_t)s;
    if (hot_from_64(s) != want)
        vio("from_64 clamp", (uint64_t)s);
}

static void vet_to32(int64_t s)
{
    uint32_t uw = (uint32_t)(uint64_t)s;
    int32_t rw = u32_to_i32(uw);
    int32_t rs = (s < INT64_C(-2147483648)) ? INT32_MIN
               : (s > INT64_C(2147483647)) ? INT32_MAX
               : (int32_t)s;
    int re = (s < INT64_C(-2147483648) || s > INT64_C(2147483647))
                 ? HOT_ERANGE : 0;
    int32_t o;
    int rc;

    rc = hot_to_32(s, HOT_WRAP, &o);
    if (rc != 0 || o != rw)
        vio("to_32 WRAP", (uint64_t)s);
    rc = hot_to_32(s, HOT_SATURATE, &o);
    if (rc != 0 || o != rs)
        vio("to_32 SATURATE", (uint64_t)s);
    rc = hot_to_32(s, HOT_ERROR, &o);
    if (rc != re || (re == 0 && o != rw))
        vio("to_32 ERROR", (uint64_t)s);
}

static void vet_u32(uint32_t u, uint32_t era_u)
{
    hot_time_t w = hot_from_u32(u, era_u);
    int64_t t = (int64_t)era_u * INT64_C(4294967296) + (int64_t)u;
    uint64_t era_want = ((uint64_t)t + UINT64_C(2147483648)) >> 32;

    if (hot_seconds(w) != t)
        vio("u32 seconds", u);
    if ((uint64_t)hot_era_of(w) != era_want)
        vio("u32 re-encoded era", u);
    if (!hot_is_compat(w))
        vio("u32 class", u);
}

static void vet_bounded(uint64_t t)
{
    uint32_t lowu = (uint32_t)t;
    int32_t legacy = u32_to_i32(lowu);
    uint64_t era_true = ((t - (uint64_t)lowu) >> 32)
                        + (lowu >= UINT64_C(0x80000000) ? 1u : 0u);
    hot_time_t out;
    int rc;

    if (era_true <= HOT_ERA_MAX) {
        uint32_t era32 = (uint32_t)era_true;
        hot_time_t w = hot_from_32(legacy, era32);
        if (hot_seconds(w) != (int64_t)t)
            vio("bounded: true-era decode", t);
    }

    rc = hot_from_32_bounded(legacy, (int64_t)t - INT64_C(2147483647),
                             (int64_t)t + INT64_C(2147483646), &out);
    if (rc != 0 || hot_seconds(out) != (int64_t)t)
        vio("bounded: unique window", t);

    rc = hot_from_32_bounded(legacy, (int64_t)t,
                             (int64_t)t + INT64_C(12884901888), &out);
    if (rc != HOT_EAMBIGUOUS)
        vio("bounded: multi-era window not refused", t);

    rc = hot_from_32_bounded(legacy, (int64_t)t + 1,
                             (int64_t)t + INT64_C(4294967295), &out);
    if (rc != HOT_EAMBIGUOUS)
        vio("bounded: empty window not refused", t);
}

int main(int argc, char **argv)
{
    uint64_t iters = (argc > 1) ? strtoull(argv[1], NULL, 0)
                                : UINT64_C(1000000);
    uint64_t nb = sizeof BND / sizeof *BND;
    uint64_t ns = sizeof S64 / sizeof *S64;

    if (argc > 2)
        rng = strtoull(argv[2], NULL, 0);

    for (uint64_t i = 0; i < iters; i++) {
        hot_time_t w = (i < nb) ? BND[i] : rnd64();
        hot_time_t w1, w2;
        int64_t s;
        uint64_t t;

        vet_word(w);

        s = (i < ns) ? S64[i] : (int64_t)rnd64();
        vet_from64(s);
        vet_to32(s);

        vet_u32((uint32_t)rnd64(), (uint32_t)rnd64() & UINT32_C(0x7FFFFFFE));
        vet_word(hot_from_u32((uint32_t)rnd64(), (uint32_t)rnd64()));

        t = ((i < nb) ? BND[i] : rnd64()) % (UINT64_C(1) << 48);
        vet_bounded(t);

        w1 = rnd64() | HOT_BIT_32COMPAT;
        w2 = rnd64() | HOT_BIT_32COMPAT;
        if ((w1 < w2) != (hot_seconds(w1) < hot_seconds(w2)))
            vio("tagged ordering", w1);
    }

    if (bad) {
        printf("FUZZ FAILED: %" PRIu64 " violations; first: %s\n",
               bad, firstbad);
        return 1;
    }
    printf("FUZZ PASSED: %" PRIu64 " iterations\n", iters);
    return 0;
}