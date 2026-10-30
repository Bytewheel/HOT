#ifndef HOT_EXT_H
#define HOT_EXT_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "hot/time.h"

/*
 * HOT extensions (normative: docs/08-extensions.md; every MUST of
 * docs/03 remains in force). Subsecond fraction records (levels 1-2),
 * deep-past class, byte-order opt-in.
 *
 * _HOT_TIME_BITS ladder (X11), mirroring _TIME_BITS: undefined/0 =
 * seconds only; 1 = + nanosecond records; 2 = + attosecond records.
 * Cumulative; the library ships built at rung 2 (src/hotext.c).
 */

#ifndef _HOT_TIME_BITS
#define _HOT_TIME_BITS 0
#endif

typedef struct { uint64_t w; } hot_word;          /* strong typedef */

#define HOT_NS_MAX         UINT32_C(1000000000)  /* exclusive bound */
#define HOT_AS_MAX         UINT64_C(1000000000)  /* exclusive bound */
#define HOT_NS_CLASS_SHIFT 30
#define HOT_NS_CLASS_MASK  UINT32_C(0xC0000000)
#define HOT_NS_CLASS_PLAIN UINT32_C(0x00000000)
#define HOT_NS_CLASS_DEEP  UINT32_C(0x40000000)
#define HOT_DEEP_BIAS      (UINT64_C(1) << 62)

hot_word   hot_word_adopt(hot_time_t word);
hot_time_t hot_word_raw(const hot_word w);
int64_t    hot_word_seconds(const hot_word w);   /* docs/03 semantics */

#if _HOT_TIME_BITS >= 1

/* ns is the record's raw ns field: class bits 31..30 (X4) plus
 * nanoseconds within the floored second, bits 29..0, < 10^9 (X2/X3).
 * The class cannot be derived from w alone -- plain-untagged and
 * deep-past words overlap in w space -- so it lives here. */
typedef struct { uint64_t w; uint32_t ns; } hot_word_ns;

int      hot_word_ns_adopt(hot_time_t word, uint32_t ns, hot_word_ns *out);
int      hot_word_ns_plain(int64_t seconds, uint32_t ns, hot_word_ns *out);
int      hot_word_ns_deep(int64_t seconds, uint32_t ns, hot_word_ns *out);
int      hot_word_ns_now(hot_word_ns *out);      /* CLOCK_REALTIME */
int      hot_word_ns_unpack(uint64_t w, uint32_t ns_raw, hot_word_ns *out);
void     hot_word_ns_pack(const hot_word_ns *r, uint64_t *w, uint32_t *ns_raw);
int64_t  hot_word_ns_seconds(const hot_word_ns *r); /* floored second */
uint32_t hot_word_ns_class(const hot_word_ns *r);
uint32_t hot_word_ns_fraction(const hot_word_ns *r);
bool     hot_word_ns_is_deep(const hot_word_ns *r);
bool     hot_word_ns_is_compat(const hot_word_ns *r);
int      hot_word_ns_cmp(const hot_word_ns *a, const hot_word_ns *b);
int      hot_word_ns_from_timespec(const struct timespec *ts, hot_word_ns *out);
int      hot_word_ns_timespec(const hot_word_ns *r, struct timespec *ts);

#endif /* _HOT_TIME_BITS >= 1 */

#if _HOT_TIME_BITS >= 2

/* Same surface with the as field; no timespec forms (attoseconds do
 * not fit struct timespec). */
typedef struct { uint64_t w; uint32_t ns; uint64_t as; } hot_word_as;

int      hot_word_as_adopt(hot_time_t word, uint32_t ns, uint64_t as,
                           hot_word_as *out);
int      hot_word_as_plain(int64_t seconds, uint32_t ns, uint64_t as,
                           hot_word_as *out);
int      hot_word_as_deep(int64_t seconds, uint32_t ns, uint64_t as,
                          hot_word_as *out);
int      hot_word_as_now(hot_word_as *out);
int      hot_word_as_unpack(uint64_t w, uint32_t ns_raw, uint64_t as,
                            hot_word_as *out);
void     hot_word_as_pack(const hot_word_as *r, uint64_t *w, uint32_t *ns_raw,
                          uint64_t *as);
int64_t  hot_word_as_seconds(const hot_word_as *r);
uint32_t hot_word_as_class(const hot_word_as *r);
uint32_t hot_word_as_fraction(const hot_word_as *r);
bool     hot_word_as_is_deep(const hot_word_as *r);
bool     hot_word_as_is_compat(const hot_word_as *r);
int      hot_word_as_cmp(const hot_word_as *a, const hot_word_as *b);

#endif /* _HOT_TIME_BITS >= 2 */

#endif
