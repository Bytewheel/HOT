#define _POSIX_C_SOURCE 199309L

#include "hot/ext.h"

#if _HOT_TIME_BITS >= 1

static int frac_ok(uint32_t ns)
{
    return ns < HOT_NS_MAX ? 0 : HOT_EMALFORMED;
}

static uint64_t deep_word(int64_t s)
{
    if (s >= 0)
        return (uint64_t)s + HOT_DEEP_BIAS;
    return HOT_DEEP_BIAS - (uint64_t)(-s);
}

static int64_t deep_sec(uint64_t w)
{
    if (w >= HOT_DEEP_BIAS)
        return (int64_t)(w - HOT_DEEP_BIAS);
    return -((int64_t)(HOT_DEEP_BIAS - w));
}

static int deep_range_ok(int64_t s)
{
    if (s < -(int64_t)HOT_DEEP_BIAS || s >= (int64_t)HOT_DEEP_BIAS)
        return HOT_ERANGE;
    return 0;
}

static void realtime_clamped(struct timespec *ts)
{
    if (clock_gettime(CLOCK_REALTIME, ts) != 0) {
        ts->tv_sec = time(NULL);
        ts->tv_nsec = 0;
    }
    if (ts->tv_sec < 0) {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
    }
}

static int timespec_ok(const struct timespec *ts)
{
    if (ts->tv_nsec < 0)
        return HOT_EMALFORMED;
    if ((uint64_t)ts->tv_nsec >= HOT_AS_MAX)
        return HOT_EMALFORMED;
    return 0;
}

#endif /* _HOT_TIME_BITS >= 1 */

#if _HOT_TIME_BITS >= 2

static int as_ok(uint64_t as)
{
    return as < HOT_AS_MAX ? 0 : HOT_EMALFORMED;
}

#endif /* _HOT_TIME_BITS >= 2 */

hot_word hot_word_adopt(hot_time_t word)
{
    hot_word r;
    r.w = word;
    return r;
}

hot_time_t hot_word_raw(const hot_word w)
{
    return w.w;
}

int64_t hot_word_seconds(const hot_word w)
{
    return hot_seconds(w.w);
}

#if _HOT_TIME_BITS >= 1

int hot_word_ns_adopt(hot_time_t word, uint32_t ns, hot_word_ns *out)
{
    if (out == NULL || frac_ok(ns) != 0)
        return HOT_EMALFORMED;
    out->w = word;
    out->ns = ns;
    return 0;
}

int hot_word_ns_plain(int64_t seconds, uint32_t ns, hot_word_ns *out)
{
    if (out == NULL || frac_ok(ns) != 0)
        return HOT_EMALFORMED;
    if (seconds < 0)
        return HOT_ERANGE;
    out->w = (uint64_t)seconds;
    out->ns = ns;
    return 0;
}

int hot_word_ns_deep(int64_t seconds, uint32_t ns, hot_word_ns *out)
{
    if (out == NULL || frac_ok(ns) != 0)
        return HOT_EMALFORMED;
    int rc = deep_range_ok(seconds);
    if (rc != 0)
        return rc;
    out->w = deep_word(seconds);
    out->ns = HOT_NS_CLASS_DEEP | ns;
    return 0;
}

int hot_word_ns_now(hot_word_ns *out)
{
    struct timespec ts;
    if (out == NULL)
        return HOT_EMALFORMED;
    realtime_clamped(&ts);
    out->w = (uint64_t)ts.tv_sec;
    out->ns = (uint32_t)ts.tv_nsec;
    return 0;
}

int hot_word_ns_unpack(uint64_t w, uint32_t ns_raw, hot_word_ns *out)
{
    uint32_t cls = ns_raw & HOT_NS_CLASS_MASK;
    if (out == NULL)
        return HOT_EMALFORMED;
    if (cls != HOT_NS_CLASS_PLAIN && cls != HOT_NS_CLASS_DEEP)
        return HOT_EMALFORMED;
    if ((ns_raw & ~HOT_NS_CLASS_MASK) >= HOT_NS_MAX)
        return HOT_EMALFORMED;
    if (cls == HOT_NS_CLASS_DEEP && (w & HOT_BIT_32COMPAT) != 0)
        return HOT_EMALFORMED;
    out->w = w;
    out->ns = ns_raw;
    return 0;
}

void hot_word_ns_pack(const hot_word_ns *r, uint64_t *w, uint32_t *ns_raw)
{
    if (r == NULL)
        return;
    if (w != NULL)
        *w = r->w;
    if (ns_raw != NULL)
        *ns_raw = r->ns;
}

int64_t hot_word_ns_seconds(const hot_word_ns *r)
{
    if ((r->ns & HOT_NS_CLASS_MASK) == HOT_NS_CLASS_DEEP)
        return deep_sec(r->w);
    return hot_seconds(r->w);
}

uint32_t hot_word_ns_class(const hot_word_ns *r)
{
    return r->ns & HOT_NS_CLASS_MASK;
}

uint32_t hot_word_ns_fraction(const hot_word_ns *r)
{
    return r->ns & ~HOT_NS_CLASS_MASK;
}

bool hot_word_ns_is_deep(const hot_word_ns *r)
{
    return (r->ns & HOT_NS_CLASS_MASK) == HOT_NS_CLASS_DEEP;
}

bool hot_word_ns_is_compat(const hot_word_ns *r)
{
    return (r->w & HOT_BIT_32COMPAT) != 0;
}

int hot_word_ns_cmp(const hot_word_ns *a, const hot_word_ns *b)
{
    int64_t sa = hot_word_ns_seconds(a);
    int64_t sb = hot_word_ns_seconds(b);
    if (sa != sb)
        return sa < sb ? -1 : 1;
    uint32_t fa = hot_word_ns_fraction(a);
    uint32_t fb = hot_word_ns_fraction(b);
    if (fa != fb)
        return fa < fb ? -1 : 1;
    return 0;
}

int hot_word_ns_from_timespec(const struct timespec *ts, hot_word_ns *out)
{
    if (ts == NULL || out == NULL)
        return HOT_EMALFORMED;
    int rc = timespec_ok(ts);
    if (rc != 0)
        return rc;
    if (ts->tv_sec >= 0) {
        out->w = (uint64_t)ts->tv_sec;
        out->ns = (uint32_t)ts->tv_nsec;
        return 0;
    }
    rc = deep_range_ok((int64_t)ts->tv_sec);
    if (rc != 0)
        return rc;
    out->w = deep_word((int64_t)ts->tv_sec);
    out->ns = HOT_NS_CLASS_DEEP | (uint32_t)ts->tv_nsec;
    return 0;
}

int hot_word_ns_timespec(const hot_word_ns *r, struct timespec *ts)
{
    if (r == NULL || ts == NULL)
        return HOT_EMALFORMED;
    int64_t s = hot_word_ns_seconds(r);
    ts->tv_sec = (time_t)s;
    if ((int64_t)ts->tv_sec != s)
        return HOT_ERANGE;
    ts->tv_nsec = (long)hot_word_ns_fraction(r);
    return 0;
}

#endif /* _HOT_TIME_BITS >= 1 */

#if _HOT_TIME_BITS >= 2

int hot_word_as_adopt(hot_time_t word, uint32_t ns, uint64_t as,
                      hot_word_as *out)
{
    if (out == NULL || frac_ok(ns) != 0 || as_ok(as) != 0)
        return HOT_EMALFORMED;
    out->w = word;
    out->ns = ns;
    out->as = as;
    return 0;
}

int hot_word_as_plain(int64_t seconds, uint32_t ns, uint64_t as,
                      hot_word_as *out)
{
    if (out == NULL || frac_ok(ns) != 0 || as_ok(as) != 0)
        return HOT_EMALFORMED;
    if (seconds < 0)
        return HOT_ERANGE;
    out->w = (uint64_t)seconds;
    out->ns = ns;
    out->as = as;
    return 0;
}

int hot_word_as_deep(int64_t seconds, uint32_t ns, uint64_t as,
                     hot_word_as *out)
{
    if (out == NULL || frac_ok(ns) != 0 || as_ok(as) != 0)
        return HOT_EMALFORMED;
    int rc = deep_range_ok(seconds);
    if (rc != 0)
        return rc;
    out->w = deep_word(seconds);
    out->ns = HOT_NS_CLASS_DEEP | ns;
    out->as = as;
    return 0;
}

int hot_word_as_now(hot_word_as *out)
{
    struct timespec ts;
    if (out == NULL)
        return HOT_EMALFORMED;
    realtime_clamped(&ts);
    out->w = (uint64_t)ts.tv_sec;
    out->ns = (uint32_t)ts.tv_nsec;
    out->as = 0;
    return 0;
}

int hot_word_as_unpack(uint64_t w, uint32_t ns_raw, uint64_t as,
                       hot_word_as *out)
{
    uint32_t cls = ns_raw & HOT_NS_CLASS_MASK;
    if (out == NULL)
        return HOT_EMALFORMED;
    if (cls != HOT_NS_CLASS_PLAIN && cls != HOT_NS_CLASS_DEEP)
        return HOT_EMALFORMED;
    if ((ns_raw & ~HOT_NS_CLASS_MASK) >= HOT_NS_MAX)
        return HOT_EMALFORMED;
    if (as_ok(as) != 0)
        return HOT_EMALFORMED;
    if (cls == HOT_NS_CLASS_DEEP && (w & HOT_BIT_32COMPAT) != 0)
        return HOT_EMALFORMED;
    out->w = w;
    out->ns = ns_raw;
    out->as = as;
    return 0;
}

void hot_word_as_pack(const hot_word_as *r, uint64_t *w, uint32_t *ns_raw,
                      uint64_t *as)
{
    if (r == NULL)
        return;
    if (w != NULL)
        *w = r->w;
    if (ns_raw != NULL)
        *ns_raw = r->ns;
    if (as != NULL)
        *as = r->as;
}

int64_t hot_word_as_seconds(const hot_word_as *r)
{
    if ((r->ns & HOT_NS_CLASS_MASK) == HOT_NS_CLASS_DEEP)
        return deep_sec(r->w);
    return hot_seconds(r->w);
}

uint32_t hot_word_as_class(const hot_word_as *r)
{
    return r->ns & HOT_NS_CLASS_MASK;
}

uint32_t hot_word_as_fraction(const hot_word_as *r)
{
    return r->ns & ~HOT_NS_CLASS_MASK;
}

bool hot_word_as_is_deep(const hot_word_as *r)
{
    return (r->ns & HOT_NS_CLASS_MASK) == HOT_NS_CLASS_DEEP;
}

bool hot_word_as_is_compat(const hot_word_as *r)
{
    return (r->w & HOT_BIT_32COMPAT) != 0;
}

int hot_word_as_cmp(const hot_word_as *a, const hot_word_as *b)
{
    int64_t sa = hot_word_as_seconds(a);
    int64_t sb = hot_word_as_seconds(b);
    if (sa != sb)
        return sa < sb ? -1 : 1;
    uint32_t fa = hot_word_as_fraction(a);
    uint32_t fb = hot_word_as_fraction(b);
    if (fa != fb)
        return fa < fb ? -1 : 1;
    if (a->as != b->as)
        return a->as < b->as ? -1 : 1;
    return 0;
}

#endif /* _HOT_TIME_BITS >= 2 */
