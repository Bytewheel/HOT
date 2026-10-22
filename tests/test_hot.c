#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "hot/time.h"

static int fails = 0;
static int checks = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (cond)
        printf("ok   %s\n", what);
    else {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void check_word(const char *what, hot_time_t got, hot_time_t want)
{
    checks++;
    if (got == want)
        printf("ok   %s = 0x%016" PRIX64 "\n", what, got);
    else {
        printf("FAIL %s = 0x%016" PRIX64 " want 0x%016" PRIX64 "\n",
               what, got, want);
        fails++;
    }
}

static void check_i64(const char *what, int64_t got, int64_t want)
{
    checks++;
    if (got == want)
        printf("ok   %s = %" PRId64 "\n", what, got);
    else {
        printf("FAIL %s = %" PRId64 " want %" PRId64 "\n", what, got, want);
        fails++;
    }
}

static void check_i32(const char *what, int ret, int32_t got, int32_t want)
{
    checks++;
    if (ret == 0 && got == want)
        printf("ok   %s = %" PRId32 "\n", what, got);
    else {
        printf("FAIL %s ret %d = %" PRId32 " want %" PRId32 "\n",
               what, ret, got, want);
        fails++;
    }
}

int main(void)
{
    puts("== hot_from_32 / hot_seconds: docs/03 worked examples ==");
    check_word("wrap-07 tagged", hot_from_32(INT32_MAX, 0),
               UINT64_C(0x800000007FFFFFFF));
    check_word("wrap-08 tagged", hot_from_32(INT32_MIN, 1),
               UINT64_C(0x8000000180000000));
    check_word("epoch tagged", hot_from_32(0, 0),
               UINT64_C(0x8000000000000000));
    check_word("1901 tagged", hot_from_32(INT32_MIN, 0),
               UINT64_C(0x8000000080000000));

    check_i64("seconds(wrap-07)", hot_seconds(hot_from_32(INT32_MAX, 0)),
              INT64_C(2147483647));
    check_i64("seconds(wrap-08)", hot_seconds(hot_from_32(INT32_MIN, 1)),
              INT64_C(2147483648));
    check_i64("seconds(1901)", hot_seconds(hot_from_32(INT32_MIN, 0)),
              INT64_C(-2147483648));
    check(hot_from_32(INT32_MAX, 0) < hot_from_32(INT32_MIN, 1),
          "tagged ordering across wrap");
    check(hot_from_32(INT32_C(123456789), 0)
              < hot_from_32(INT32_C(123456789), 1),
          "same low value, era 0 < era 1");

    puts("== hot_from_64 (untagged, R1) ==");
    check_word("from_64(0)", hot_from_64(0), 0);
    check_word("from_64(1)", hot_from_64(1), 1);
    check_word("from_64(2^62)", hot_from_64(INT64_C(4611686018427387904)),
              UINT64_C(4611686018427387904));
    check_word("from_64(-1) clamps to 0", hot_from_64(-1), 0);
    check_word("from_64(INT64_MIN) clamps to 0", hot_from_64(INT64_MIN), 0);
    check(!hot_is_compat(hot_from_64(INT64_MAX)),
          "from_64 keeps tag bit clear");

    puts("== hot_seconds / untagged extremes ==");
    check_i64("seconds(INT64_MAX untagged)",
              hot_seconds(UINT64_C(0x7FFFFFFFFFFFFFFF)), INT64_MAX);
    check_i64("seconds(0)", hot_seconds(0), 0);
    check(hot_seconds(hot_from_64(INT64_C(1234567890))) == INT64_C(1234567890),
          "seconds(from_64(s)) == s for 1234567890");

    puts("== hot_is_compat ==");
    check(hot_is_compat(hot_from_32(0, 0)), "is_compat(from_32)");
    check(hot_is_compat(hot_from_u32(0, 0)), "is_compat(from_u32)");
    check(!hot_is_compat(hot_from_64(0)), "!is_compat(from_64)");
    check(!hot_is_compat(0), "!is_compat(0)");
    check(hot_is_compat(HOT_BIT_32COMPAT), "is_compat(tag bit alone)");

    puts("== hot_era_of (both classes) ==");
    check_i64("era_of(from_64(0)) == 0", (int64_t)hot_era_of(hot_from_64(0)),
              0);
    check_i64("era_of(from_64(INT32_MAX)) == 0",
              (int64_t)hot_era_of(hot_from_64(INT32_MAX)), 0);
    check_i64("era_of(from_64(2^31)) == 1",
              (int64_t)hot_era_of(hot_from_64(INT64_C(2147483648))), 1);
    check_i64("era_of(from_64(2^32)) == 1",
              (int64_t)hot_era_of(hot_from_64(INT64_C(4294967296))), 1);
    check_i64("era_of(max untagged) == 0x80000000",
              (int64_t)hot_era_of(UINT64_C(0x7FFFFFFFFFFFFFFF)),
              INT64_C(0x80000000));
    check_i64("era_of(wrap-07 word) == 0",
              (int64_t)hot_era_of(hot_from_32(INT32_MAX, 0)), 0);
    check_i64("era_of(wrap-08 word) == 1",
              (int64_t)hot_era_of(hot_from_32(INT32_MIN, 1)), 1);
    check(hot_era_of(hot_from_32(INT32_MAX, 0))
              == hot_era_of(hot_from_64(INT32_MAX)),
          "era_of cross-class agree at INT32_MAX");

    puts("== hot_from_u32 (unsigned sources, R5/R8) ==");
    check_i64("seconds(u32 0x80000000 era_u 0)",
              hot_seconds(hot_from_u32(UINT32_C(0x80000000), 0)),
              INT64_C(2147483648));
    check_i64("era field of u32 0x80000000 era_u 0",
              (int64_t)hot_era_of(hot_from_u32(UINT32_C(0x80000000), 0)), 1);
    check_word("u32 0x80000000 era_u 0 word == wrap-08 word",
                hot_from_u32(UINT32_C(0x80000000), 0),
                hot_from_32(INT32_MIN, 1));
    check_i64("seconds(u32 0xFFFFFFFF era_u 0)",
              hot_seconds(hot_from_u32(UINT32_MAX, 0)), INT64_C(4294967295));
    check_word("u32 0xFFFFFFFF era_u 0 word == from_32(-1, 1)",
                hot_from_u32(UINT32_MAX, 0), hot_from_32((int32_t)-1, 1));
    check_i64("seconds(u32 0 era_u 1)", hot_seconds(hot_from_u32(0, 1)),
              INT64_C(4294967296));

    puts("== hot_from_32_bounded (R4) ==");
    hot_time_t wout = 0;
    check(hot_from_32_bounded(INT32_C(0x76543210), INT64_C(4294967296),
                              INT64_C(8589934592), &wout) == 0,
          "bounded: [2^32, 2^33] selects era 1 uniquely");
    check_word("  -> word", wout, hot_from_32(INT32_C(0x76543210), 1));
    check(hot_from_32_bounded(INT32_C(0x76543210), 0,
                              INT64_C(8589934592), &wout) == HOT_EAMBIGUOUS,
          "bounded: [0, 2^33] spans eras 0 and 1 -> -HOT_EAMBIGUOUS");
    check(hot_from_32_bounded(INT32_C(0x76543210), 5, 10, &wout)
              == HOT_EAMBIGUOUS,
          "bounded: [5, 10] selects no era -> -HOT_EAMBIGUOUS");
    check(hot_from_32_bounded(0, INT64_MIN, INT64_MAX, &wout) == HOT_EAMBIGUOUS,
          "bounded: [INT64_MIN, INT64_MAX] selects all -> -HOT_EAMBIGUOUS");
    check(hot_from_32_bounded(-2, -10, 10, &wout) == 0,
          "bounded: legacy -2 in [-10, 10] selects era 0 (pre-1970)");
    check_word("  -> word", wout, hot_from_32(-2, 0));
    check_i64("  -> seconds", hot_seconds(wout), -2);
    check(hot_from_32_bounded(0, INT64_C(4294967296), INT64_C(4294967296),
                              &wout) == 0,
          "bounded: [2^32, 2^32] single instant selects era 1");
    check_word("  -> word", wout, hot_from_32(0, 1));

    puts("== hot_to_32 (legacy sinks) ==");
    int32_t v32 = 0;
    int rc = hot_to_32(INT64_C(2147483648), HOT_WRAP, &v32);
    check_i32("to_32(2^31, WRAP)", rc, v32, INT32_MIN);
    rc = hot_to_32(INT64_C(4294967295), HOT_WRAP, &v32);
    check_i32("to_32(2^32-1, WRAP)", rc, v32, (int32_t)-1);
    rc = hot_to_32(INT64_C(4294967296), HOT_WRAP, &v32);
    check_i32("to_32(2^32, WRAP)", rc, v32, 0);
    rc = hot_to_32(-1, HOT_WRAP, &v32);
    check_i32("to_32(-1, WRAP)", rc, v32, (int32_t)-1);
    rc = hot_to_32(INT32_MIN, HOT_SATURATE, &v32);
    check_i32("to_32(INT32_MIN, SATURATE)", rc, v32, INT32_MIN);
    rc = hot_to_32(INT64_MIN, HOT_SATURATE, &v32);
    check_i32("to_32(INT64_MIN, SATURATE)", rc, v32, INT32_MIN);
    rc = hot_to_32(INT64_MAX, HOT_SATURATE, &v32);
    check_i32("to_32(INT64_MAX, SATURATE)", rc, v32, INT32_MAX);
    rc = hot_to_32(123, HOT_ERROR, &v32);
    check_i32("to_32(123, ERROR)", rc, v32, 123);
    check(hot_to_32(INT64_C(2147483648), HOT_ERROR, &v32) == HOT_ERANGE,
          "to_32(2^31, ERROR) -> -HOT_ERANGE");
    check(hot_to_32(INT64_C(-2147483649), HOT_ERROR, &v32) == HOT_ERANGE,
          "to_32(-2^31-1, ERROR) -> -HOT_ERANGE");

    puts("== hot_now_word / hot_compat_active ==");
    hot_time_t n1 = hot_now_word();
    hot_time_t n2 = hot_now_word();
    check(!hot_is_compat(n1), "now_word: tag bit clear");
    check(hot_seconds(n1) >= INT64_C(1600000000),
          "now_word: seconds >= 1600000000");
    check(hot_seconds(n2) >= hot_seconds(n1),
          "now_word: monotonic over calls");
    check(hot_compat_active() == 0,
          "compat_active: 0 on 64-bit time_t host");

    puts("== T2 property: int32 round-trip / era / ordering, eras 0-3 ==");
    static const int32_t vs32[] = {
        INT32_MIN, INT32_MIN + 1, -2, -1, 0, 1, 2, INT32_MAX - 1, INT32_MAX
    };
    uint64_t bad = 0;
    for (uint32_t era = 0; era <= 3; era++) {
        for (size_t i = 0; i < sizeof vs32 / sizeof *vs32; i++) {
            int32_t v = vs32[i];
            hot_time_t w = hot_from_32(v, era);
            if (hot_seconds(w) != (int64_t)era * INT64_C(4294967296) + v)
                bad++;
            if (!hot_is_compat(w))
                bad++;
            if (hot_era_of(w) != era)
                bad++;
            if (era < 3 && !(w < hot_from_32(v, era + 1)))
                bad++;
        }
    }
    check(bad == 0, "int32 round-trip/era/order (36 cases)");

    puts("== T2 property: u32 round-trip / re-encoded era, era_u 0-3 ==");
    static const uint32_t vsu[] = {
        0, 1, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000001,
        0xFFFFFFFE, UINT32_MAX
    };
    bad = 0;
    for (uint32_t era_u = 0; era_u <= 3; era_u++) {
        for (size_t i = 0; i < sizeof vsu / sizeof *vsu; i++) {
            uint32_t u = vsu[i];
            hot_time_t w = hot_from_u32(u, era_u);
            int64_t want = (int64_t)era_u * INT64_C(4294967296) + (int64_t)u;
            if (hot_seconds(w) != want)
                bad++;
            if (hot_era_of(w)
                != (uint32_t)((want + INT64_C(2147483648))
                               / INT64_C(4294967296)))
                bad++;
            if (i > 0 && !(hot_from_u32(vsu[i - 1], era_u) < w))
                bad++;
        }
    }
    check(bad == 0, "u32 round-trip/era/order (32 cases)");

    printf("%s: %d failures / %d checks\n",
           fails ? "TEST FAILED" : "TEST PASSED", fails, checks);
    return fails != 0;
}