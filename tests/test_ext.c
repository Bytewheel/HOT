#define _POSIX_C_SOURCE 199309L

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "hot/ext.h"
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

static uint64_t rng = UINT64_C(0x5EED11D5C0FFEE0D);

static uint64_t rnd64(void)
{
    uint64_t x = rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return rng = x;
}

static uint64_t deep_want(int64_t s)
{
    if (s >= 0)
        return HOT_DEEP_BIAS + (uint64_t)s;
    return HOT_DEEP_BIAS - (uint64_t)(-s);
}

static int vet_ns(uint64_t wv, uint32_t raw, int64_t sec, uint32_t frac,
                  uint32_t cls, bool deep, bool compat)
{
    hot_word_ns r = {0, 0};
    uint64_t w = 1;
    uint32_t raw2 = 1;

    if (hot_word_ns_unpack(wv, raw, &r) != 0)
        return 1;
    if (r.w != wv || r.ns != raw)
        return 1;
    if (hot_word_ns_seconds(&r) != sec)
        return 1;
    if (hot_word_ns_fraction(&r) != frac)
        return 1;
    if (hot_word_ns_class(&r) != cls)
        return 1;
    if (hot_word_ns_is_deep(&r) != deep || hot_word_ns_is_compat(&r) != compat)
        return 1;
    hot_word_ns_pack(&r, &w, &raw2);
    if (w != wv || raw2 != raw)
        return 1;
    return 0;
}

int main(void)
{
    const hot_time_t tagw = UINT64_C(0x8000000080000000); /* era 0, -2^31 */
    uint64_t bad = 0;

    puts("== level-0 hot_word: adopt/raw/seconds ==");
    hot_word hw = hot_word_adopt(hot_from_32(INT32_MAX, 0));
    check(hot_word_raw(hw) == UINT64_C(0x800000007FFFFFFF)
              && hot_word_seconds(hw) == INT64_C(2147483647),
          "adopt/raw/seconds: wrap-07 tagged");
    hw = hot_word_adopt(UINT64_C(0x000000007FFFFFFF));
    check(hot_word_raw(hw) == UINT64_C(0x000000007FFFFFFF)
              && hot_word_seconds(hw) == INT64_C(2147483647),
          "adopt/raw/seconds: 2038 untagged");

    puts("== docs/08 worked examples, level 1 (typed fields + pack) ==");
    check(vet_ns(UINT64_C(0x000000007FFFFFFF), UINT32_C(0x1DCD6500),
                 INT64_C(2147483647), UINT32_C(500000000),
                 HOT_NS_CLASS_PLAIN, false, false) == 0,
          "worked example: 2038-01-19 03:14:07.5 plain untagged");
    check(vet_ns(UINT64_C(0x8000000180000000), UINT32_C(0x0EE6B280),
                 INT64_C(2147483648), UINT32_C(250000000),
                 HOT_NS_CLASS_PLAIN, false, true) == 0,
          "worked example: 2038-01-19 03:14:08.25 tagged era 1");
    check(vet_ns(UINT64_C(0x3FFFFFFF80000000), UINT32_C(0x6CB41780),
                 INT64_C(-2147483648), UINT32_C(750000000),
                 HOT_NS_CLASS_DEEP, true, false) == 0,
          "worked example: 1901-12-13 20:45:52.75 deep-past");
    check(vet_ns(UINT64_C(0x3FFFFFFFFFFFFFFF), UINT32_C(0x5DCD6500),
                 INT64_C(-1), UINT32_C(500000000),
                 HOT_NS_CLASS_DEEP, true, false) == 0,
          "worked example: 1969-12-31 23:59:59.5 deep-past (floor -1 s)");
    check(vet_ns(UINT64_C(0x80000000FFFFFFFF), UINT32_C(0x1DCD6500),
                 INT64_C(-1), UINT32_C(500000000),
                 HOT_NS_CLASS_PLAIN, false, true) == 0,
          "worked example: -0.5 s tagged alternative (era 0, -1 s)");

    hot_word_ns e = {0, 0};
    check(hot_word_ns_plain(INT64_C(2147483647), UINT32_C(500000000), &e) == 0
              && e.w == UINT64_C(0x000000007FFFFFFF)
              && e.ns == UINT32_C(0x1DCD6500),
          "constructor: _plain(2147483647, 5e8) = example 1");
    check(hot_word_ns_adopt(hot_from_32(INT32_MIN, 1), UINT32_C(250000000),
                            &e) == 0
              && e.w == UINT64_C(0x8000000180000000)
              && e.ns == UINT32_C(0x0EE6B280),
          "constructor: adopt(wrap-08 tagged, 2.5e8) = example 2");
    check(hot_word_ns_deep(INT64_C(-2147483648), UINT32_C(750000000), &e) == 0
              && e.w == UINT64_C(0x3FFFFFFF80000000)
              && e.ns == UINT32_C(0x6CB41780),
          "constructor: _deep(-2^31, 7.5e8) = example 3");
    check(hot_word_ns_deep(-1, UINT32_C(500000000), &e) == 0
              && e.w == UINT64_C(0x3FFFFFFFFFFFFFFF)
              && e.ns == UINT32_C(0x5DCD6500),
          "constructor: _deep(-1, 5e8) = example 4");

    puts("== docs/08 rejections (X3/X4) ==");
    check(hot_word_ns_unpack(0, UINT32_C(0x3B9ACA00), &e) == HOT_EMALFORMED,
          "ns raw 0x3B9ACA00 (fraction 10^9, fits 30 bits) -> -HOT_EMALFORMED");
    hot_word_as ea = {0, 0, 0};
    check(hot_word_as_unpack(0, 0, UINT64_C(1000000000), &ea)
              == HOT_EMALFORMED,
          "as = 10^9 -> -HOT_EMALFORMED");
    check(hot_word_ns_from_timespec(NULL, &e) == HOT_EMALFORMED
              && hot_word_ns_timespec(NULL, NULL) == HOT_EMALFORMED
              && hot_word_ns_unpack(0, 0, NULL) == HOT_EMALFORMED
              && hot_word_ns_plain(0, 0, NULL) == HOT_EMALFORMED
              && hot_word_ns_deep(0, 0, NULL) == HOT_EMALFORMED
              && hot_word_ns_adopt(0, 0, NULL) == HOT_EMALFORMED,
          "NULL out pointers -> -HOT_EMALFORMED");

    puts("== X2 floor semantics on negative instants ==");
    struct timespec ts = {-1, 500000000};
    check(hot_word_ns_from_timespec(&ts, &e) == 0
              && hot_word_ns_is_deep(&e)
              && e.w == UINT64_C(0x3FFFFFFFFFFFFFFF)
              && e.ns == UINT32_C(0x5DCD6500),
          "from_timespec(-0.5 s) -> deep example 4 (floor -1 s, frac 0.5 s)");
    struct timespec back = {1, 1};
    check(hot_word_ns_timespec(&e, &back) == 0 && back.tv_sec == -1
              && back.tv_nsec == 500000000,
          "timespec(deep -0.5 s) exact");
    struct timespec ts2 = {INT64_C(-2147483648), 750000000};
    check(hot_word_ns_from_timespec(&ts2, &e) == 0
              && e.w == UINT64_C(0x3FFFFFFF80000000)
              && e.ns == UINT32_C(0x6CB41780),
          "from_timespec(1901-12-13 20:45:52.75) -> deep example 3");
    struct timespec ts3 = {INT64_C(2147483647), 999999999};
    check(hot_word_ns_from_timespec(&ts3, &e) == 0
              && hot_word_ns_class(&e) == HOT_NS_CLASS_PLAIN
              && hot_word_ns_seconds(&e) == INT64_C(2147483647)
              && hot_word_ns_fraction(&e) == UINT32_C(999999999)
              && hot_word_ns_timespec(&e, &back) == 0
              && back.tv_sec == INT64_C(2147483647)
              && back.tv_nsec == 999999999,
          "from_timespec/timespec plain round-trip at 2038 boundary fraction");
    struct timespec ts4 = {0, 0};
    check(hot_word_ns_from_timespec(&ts4, &e) == 0 && e.w == 0 && e.ns == 0,
          "from_timespec(epoch) -> plain 0");
    struct timespec ts5 = {5, 1000000000};
    check(hot_word_ns_from_timespec(&ts5, &e) == HOT_EMALFORMED,
          "from_timespec tv_nsec = 10^9 -> -HOT_EMALFORMED");
    struct timespec ts6 = {5, -1};
    check(hot_word_ns_from_timespec(&ts6, &e) == HOT_EMALFORMED,
          "from_timespec tv_nsec < 0 -> -HOT_EMALFORMED");

    puts("== T11: fraction boundaries, level 1, plain/tagged/deep ==");
    static const uint32_t FRAC_OK[] = {
        0, 1, 2, HOT_NS_MAX - 2, HOT_NS_MAX - 1
    };
    static const uint32_t FRAC_BAD[] = {HOT_NS_MAX, HOT_NS_MAX + 1};
    bad = 0;
    for (size_t i = 0; i < sizeof FRAC_OK / sizeof *FRAC_OK; i++) {
        uint32_t v = FRAC_OK[i];
        hot_word_ns a = {0, 0}, b = {0, 0}, c = {0, 0}, d = {0, 0};
        if (hot_word_ns_plain(0, v, &a) != 0 || a.w != 0 || a.ns != v)
            bad++;
        if (hot_word_ns_deep(-1, v, &b) != 0
            || b.w != UINT64_C(0x3FFFFFFFFFFFFFFF)
            || b.ns != (HOT_NS_CLASS_DEEP | v))
            bad++;
        if (hot_word_ns_adopt(tagw, v, &c) != 0 || c.w != tagw || c.ns != v)
            bad++;
        if (hot_word_ns_unpack(tagw, v, &d) != 0 || d.w != tagw || d.ns != v)
            bad++;
        if (hot_word_ns_unpack(0, HOT_NS_CLASS_DEEP | v, &d) != 0
            || d.ns != (HOT_NS_CLASS_DEEP | v))
            bad++;
    }
    check(bad == 0,
          "boundaries 0/1/2/10^9-2/10^9-1 accepted, all classes (25 cases)");
    bad = 0;
    for (size_t i = 0; i < sizeof FRAC_BAD / sizeof *FRAC_BAD; i++) {
        uint32_t v = FRAC_BAD[i];
        hot_word_ns a = {0, 0};
        if (hot_word_ns_plain(0, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_ns_deep(-1, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_ns_adopt(tagw, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_ns_unpack(0, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_ns_unpack(0, HOT_NS_CLASS_DEEP | v, &a) != HOT_EMALFORMED)
            bad++;
    }
    check(bad == 0,
          "fraction 10^9 and 10^9+1 rejected -HOT_EMALFORMED, all constructors (10 cases)");

    puts("== T11: reserved classes 10/11 (X4) ==");
    static const uint32_t CLS_BAD[] = {
        UINT32_C(0x80000000), UINT32_C(0x80000001),
        UINT32_C(0xC0000000), UINT32_MAX
    };
    bad = 0;
    for (size_t i = 0; i < sizeof CLS_BAD / sizeof *CLS_BAD; i++) {
        uint32_t v = CLS_BAD[i];
        hot_word_ns a = {0, 0};
        hot_word_as b = {0, 0, 0};
        if (hot_word_ns_unpack(0, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_ns_unpack(UINT64_C(0x3FFFFFFFFFFFFFFF), v, &a)
            != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_unpack(0, v, 0, &b) != HOT_EMALFORMED)
            bad++;
    }
    check(bad == 0, "ns raw >= 0x80000000 rejected -HOT_EMALFORMED (12 cases)");

    puts("== T11: fraction/as boundaries, level 2 ==");
    bad = 0;
    for (size_t i = 0; i < sizeof FRAC_OK / sizeof *FRAC_OK; i++) {
        uint32_t v = FRAC_OK[i];
        hot_word_as a = {0, 0, 0}, b = {0, 0, 0}, c = {0, 0, 0}, d = {0, 0, 0};
        if (hot_word_as_plain(0, v, v, &a) != 0
            || a.w != 0 || a.ns != v || a.as != v)
            bad++;
        if (hot_word_as_deep(-1, v, v, &b) != 0
            || b.ns != (HOT_NS_CLASS_DEEP | v) || b.as != v)
            bad++;
        if (hot_word_as_adopt(tagw, v, v, &c) != 0
            || c.w != tagw || c.ns != v || c.as != v)
            bad++;
        if (hot_word_as_unpack(0, HOT_NS_CLASS_DEEP | v, v, &d) != 0
            || d.ns != (HOT_NS_CLASS_DEEP | v) || d.as != v)
            bad++;
    }
    check(bad == 0,
          "boundaries accepted at level 2, plain/tagged/deep (20 cases)");
    bad = 0;
    for (size_t i = 0; i < sizeof FRAC_BAD / sizeof *FRAC_BAD; i++) {
        uint32_t v = FRAC_BAD[i];
        hot_word_as a = {0, 0, 0};
        if (hot_word_as_plain(0, v, 0, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_plain(0, 0, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_deep(-1, v, 0, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_adopt(0, 0, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_unpack(0, v, 0, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_unpack(0, 0, v, &a) != HOT_EMALFORMED)
            bad++;
        if (hot_word_as_unpack(0, HOT_NS_CLASS_DEEP | v, 0, &a)
            != HOT_EMALFORMED)
            bad++;
    }
    if (hot_word_as_plain(0, 0, UINT64_MAX, &ea) != HOT_EMALFORMED)
        bad++;
    if (hot_word_as_unpack(0, 0, UINT64_MAX, &ea) != HOT_EMALFORMED)
        bad++;
    check(bad == 0,
          "ns/as >= 10^9 and as = UINT64_MAX rejected -HOT_EMALFORMED (30 cases)");

    puts("== X8: ordering within class; cross-class via conversion ==");
    hot_word_ns deep1901 = {0, 0}, plain2038 = {0, 0};
    hot_word_ns deephalf = {0, 0}, taggedhalf = {0, 0};
    check(hot_word_ns_deep(INT64_C(-2147483648), UINT32_C(750000000),
                           &deep1901) == 0
              && hot_word_ns_plain(INT64_C(2147483647), UINT32_C(500000000),
                                   &plain2038) == 0,
          "X8 setup: 1901 deep / 2038 plain records");
    check(deep1901.w > plain2038.w,
          "X8 hazard: raw deep 1901 word > raw plain 2038 word");
    check(hot_word_ns_cmp(&deep1901, &plain2038) < 0,
          "X8: cmp(deep 1901, plain 2038) < 0 via conversion");
    check(hot_word_ns_deep(-1, UINT32_C(500000000), &deephalf) == 0
              && hot_word_ns_adopt(UINT64_C(0x80000000FFFFFFFF),
                                   UINT32_C(500000000), &taggedhalf) == 0,
          "X8 setup: -0.5 s deep and tagged records");
    check(hot_word_ns_cmp(&deephalf, &taggedhalf) == 0,
          "X8: deep -0.5 s == tagged -0.5 s (same instant)");

    puts("== T11 property: 100000 random samples, levels 1-2 (fixed seed) ==");
    for (uint32_t i = 0; i < 100000; i++) {
        uint32_t frac = (uint32_t)(rnd64() % HOT_NS_MAX);
        uint32_t asv = (uint32_t)(rnd64() % HOT_AS_MAX);
        int64_t sp = (int64_t)(rnd64() & UINT64_C(0x7FFFFFFFFFFFFFFF));
        int64_t sd = (int64_t)(rnd64() >> 2) - (int64_t)HOT_DEEP_BIAS;
        int32_t low32 = (int32_t)rnd64();
        uint32_t erav = (uint32_t)rnd64() & HOT_ERA_MASK;
        hot_time_t tw = hot_from_32(low32, erav);
        int64_t tsec = hot_seconds(tw);
        hot_word_ns p = {0, 0}, q = {0, 0}, d = {0, 0}, t = {0, 0}, p2 = {0, 0};
        hot_word_as pa = {0, 0, 0}, qa = {0, 0, 0}, da = {0, 0, 0};
        hot_word_as ta = {0, 0, 0}, p0 = {0, 0, 0}, a2 = {0, 0, 0};
        uint64_t w = 0, asb = 0;
        uint32_t raw = 0;

        if (hot_word_ns_plain(sp, frac, &p) != 0 || p.w != (uint64_t)sp
            || p.ns != frac || hot_word_ns_class(&p) != HOT_NS_CLASS_PLAIN
            || hot_word_ns_is_deep(&p) || hot_word_ns_is_compat(&p)
            || hot_word_ns_seconds(&p) != sp
            || hot_word_ns_fraction(&p) != frac)
            bad++;
        hot_word_ns_pack(&p, &w, &raw);
        if (w != (uint64_t)sp || raw != frac
            || hot_word_ns_unpack(w, raw, &q) != 0 || q.w != p.w
            || q.ns != p.ns)
            bad++;

        if (hot_word_ns_adopt(tw, frac, &t) != 0 || t.w != tw || t.ns != frac
            || !hot_word_ns_is_compat(&t) || hot_word_ns_is_deep(&t)
            || hot_word_ns_seconds(&t) != tsec
            || hot_word_ns_fraction(&t) != frac)
            bad++;
        hot_word_ns_pack(&t, &w, &raw);
        if (hot_word_ns_unpack(w, raw, &q) != 0 || q.w != tw || q.ns != frac)
            bad++;

        if (hot_word_ns_deep(sd, frac, &d) != 0
            || d.w != deep_want(sd)
            || d.ns != (HOT_NS_CLASS_DEEP | frac)
            || !hot_word_ns_is_deep(&d) || hot_word_ns_is_compat(&d)
            || hot_word_ns_seconds(&d) != sd
            || hot_word_ns_fraction(&d) != frac)
            bad++;
        hot_word_ns_pack(&d, &w, &raw);
        if (hot_word_ns_unpack(w, raw, &q) != 0 || q.w != d.w || q.ns != d.ns)
            bad++;

        if (hot_word_as_plain(sp, frac, asv, &pa) != 0 || pa.w != p.w
            || pa.ns != p.ns || pa.as != asv)
            bad++;
        hot_word_as_pack(&pa, &w, &raw, &asb);
        if (w != p.w || raw != p.ns || asb != asv
            || hot_word_as_unpack(w, raw, asb, &qa) != 0 || qa.w != pa.w
            || qa.ns != pa.ns || qa.as != pa.as)
            bad++;
        if (hot_word_as_plain(sp, frac, 0, &p0) != 0 || p0.w != p.w
            || p0.ns != p.ns)
            bad++;
        if (hot_word_as_deep(sd, frac, asv, &da) != 0 || da.w != d.w
            || da.ns != d.ns || da.as != asv)
            bad++;
        if (hot_word_as_adopt(tw, frac, asv, &ta) != 0 || ta.w != tw
            || ta.ns != frac || ta.as != asv)
            bad++;

        if (hot_word_ns_cmp(&p, &p) != 0)
            bad++;
        {
            uint32_t f2 = (uint32_t)(rnd64() % HOT_NS_MAX);
            int64_t s2 = (int64_t)(rnd64() & UINT64_C(0x7FFFFFFFFFFFFFFF));
            int want = (sp != s2) ? (sp < s2 ? -1 : 1)
                       : (frac != f2) ? (frac < f2 ? -1 : 1) : 0;
            if (hot_word_ns_plain(s2, f2, &p2) != 0
                || hot_word_ns_cmp(&p, &p2) != want)
                bad++;
        }
        {
            int want = (sp != sd) ? (sp < sd ? -1 : 1) : 0;
            if (hot_word_ns_cmp(&p, &d) != want)
                bad++;
        }
        {
            int want = (tsec != sd) ? (tsec < sd ? -1 : 1) : 0;
            if (hot_word_ns_cmp(&t, &d) != want)
                bad++;
        }
        {
            int want = (sp != tsec) ? (sp < tsec ? -1 : 1) : 0;
            if (hot_word_ns_cmp(&p, &t) != want)
                bad++;
        }
        {
            uint64_t as2 = (asv + 1) % HOT_AS_MAX;
            int want = asv < as2 ? -1 : 1;
            if (hot_word_as_plain(sp, frac, as2, &a2) != 0
                || hot_word_as_cmp(&pa, &a2) != want)
                bad++;
        }
    }
    check(bad == 0,
          "pack/unpack round-trip, classes, bias, ordering, as tie-break");

    puts("== _now: canonical plain records ==");
    hot_word_ns nrec = {1, 1};
    check(hot_word_ns_now(&nrec) == 0
              && hot_word_ns_class(&nrec) == HOT_NS_CLASS_PLAIN
              && !hot_word_ns_is_deep(&nrec)
              && hot_word_ns_fraction(&nrec) < HOT_NS_MAX,
          "ns_now: plain class, canonical fraction");
    hot_word_as arec = {1, 1, 1};
    check(hot_word_as_now(&arec) == 0 && arec.as == 0
              && hot_word_as_class(&arec) == HOT_NS_CLASS_PLAIN
              && !hot_word_as_is_deep(&arec)
              && hot_word_as_fraction(&arec) < HOT_NS_MAX,
          "as_now: plain class, canonical fraction, as = 0");

    printf("%s: %d failures / %d checks\n",
           fails ? "TEST FAILED" : "TEST PASSED", fails, checks);
    return fails != 0;
}
