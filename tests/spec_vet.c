#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

typedef uint64_t hot_time_t;

#define HOT_BIT_32COMPAT (UINT64_C(1) << 63)
#define HOT_ERA_SHIFT 32
#define HOT_ERA_MASK (UINT64_C(0x7FFFFFFF))

static hot_time_t hot_from_32(int32_t v, uint32_t era) {
    return HOT_BIT_32COMPAT | ((uint64_t)(era & HOT_ERA_MASK) << HOT_ERA_SHIFT) | ((uint32_t)v);
}

static hot_time_t hot_from_u32(uint32_t u, uint32_t era_u) {
    int64_t t = (int64_t)era_u * INT64_C(4294967296) + (int64_t)u;
    uint32_t era = (uint32_t)((t + INT64_C(2147483648)) / INT64_C(4294967296));
    int32_t v = (int32_t)(uint32_t)t;
    return HOT_BIT_32COMPAT | ((uint64_t)era << HOT_ERA_SHIFT) | ((uint32_t)v);
}

static int64_t hot_seconds(hot_time_t w) {
    if (!(w & HOT_BIT_32COMPAT)) return (int64_t)(w & UINT64_C(0x7FFFFFFFFFFFFFFF));
    uint32_t era = (uint32_t)((w >> HOT_ERA_SHIFT) & HOT_ERA_MASK);
    int32_t v = (int32_t)(uint32_t)(w & UINT64_C(0xFFFFFFFF));
    return (int64_t)era * INT64_C(4294967296) + v;
}

static int fails = 0;
static void chk(const char *what, hot_time_t got, hot_time_t want) {
    if (got == want) { printf("ok   %s = 0x%016" PRIX64 "\n", what, got); }
    else { printf("FAIL %s = 0x%016" PRIX64 " want 0x%016" PRIX64 "\n", what, got, want); fails++; }
}

int main(void) {
    chk("wrap-07 tagged",   hot_from_32(INT32_MAX, 0), UINT64_C(0x800000007FFFFFFF));
    chk("wrap-08 tagged",   hot_from_32((int32_t)INT32_C(-2147483648), 1), UINT64_C(0x8000000180000000));
    chk("epoch tagged",     hot_from_32(0, 0), UINT64_C(0x8000000000000000));
    chk("1901 tagged",      hot_from_32(INT32_MIN, 0), UINT64_C(0x8000000080000000));

    if (hot_seconds(hot_from_32(INT32_MAX, 0)) != 2147483647) { printf("FAIL seconds wrap-07\n"); fails++; } else puts("ok   seconds(wrap-07) = 2147483647");
    if (hot_seconds(hot_from_32((int32_t)INT32_C(-2147483648), 1)) != 2147483648LL) { printf("FAIL seconds wrap-08\n"); fails++; } else puts("ok   seconds(wrap-08) = 2147483648");
    if (hot_seconds(hot_from_32(INT32_MIN, 0)) != INT64_C(-2147483648)) { printf("FAIL seconds 1901\n"); fails++; } else puts("ok   seconds(1901) = -2147483648");

    hot_time_t a = hot_from_32(INT32_MAX, 0), b = hot_from_32((int32_t)INT32_C(-2147483648), 1);
    if (!(a < b)) { printf("FAIL ordering across wrap\n"); fails++; } else puts("ok   tagged ordering across wrap");

    hot_time_t u1 = hot_from_u32(UINT32_C(0x80000000), 0);
    if (hot_seconds(u1) != 2147483648LL) { printf("FAIL u32 era-1 mapping: %" PRId64 "\n", hot_seconds(u1)); fails++; }
    else puts("ok   u32 0x80000000 era0 -> seconds 2147483648 (signed era 1)");
    if (((u1 >> HOT_ERA_SHIFT) & HOT_ERA_MASK) != 1) { printf("FAIL u32 era encode\n"); fails++; } else puts("ok   u32 0x80000000 era0 -> signed era 1 on word");

    hot_time_t pre = hot_from_32(123456789, 0), post = hot_from_32(123456789, 1);
    if (!(pre < post)) { printf("FAIL era ordering\n"); fails++; } else puts("ok   same low value, era 0 < era 1");

    printf("%s: %d failures\n", fails ? "VET FAILED" : "VET PASSED", fails);
    return fails != 0;
}
