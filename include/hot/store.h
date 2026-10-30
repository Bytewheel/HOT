#ifndef HOT_STORE_H
#define HOT_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "hot/ext.h"

#define HOT_STORE_VER     UINT8_C(1)
#define HOT_STORE_F_WORDS UINT8_C(0x01)
#define HOT_STORE_UNIT_SEC UINT8_C(0)
#define HOT_STORE_WIDTH   UINT8_C(8)
#define HOT_STORE_HDR     ((size_t)12)

/* v2 (docs/08): fraction records + byte-order opt-in. Flags/width
 * MUST be consistent: 8 with neither fraction bit, 12 with NS only,
 * 16 with NS+AS. Entry count is LE in every mode (X9). */
#define HOT_STORE_VER_V2        UINT8_C(2)
#define HOT_STORE_F_NS          UINT8_C(0x02)
#define HOT_STORE_F_AS          UINT8_C(0x04)
#define HOT_STORE_F_BE          UINT8_C(0x08)
#define HOT_STORE_F_RESERVED_V2 UINT8_C(0xF0)

int hot_store_encode(uint8_t *buf, size_t cap, const hot_time_t *words,
                     uint32_t count, size_t *out);
int hot_store_decode(const uint8_t *buf, size_t cap, hot_time_t *words,
                     uint32_t count_max, uint32_t *count);
int hot_store_legacy32(const int32_t *stamps, uint32_t count,
                       int64_t lower, int64_t upper, hot_time_t *out);

/* v2, words only (width 8), LE or BE per be. */
int hot_store_v2_encode_words(uint8_t *buf, size_t cap, const hot_time_t *words,
                              uint32_t count, uint8_t be, size_t *out);

#if _HOT_TIME_BITS >= 1

/* v2 typed per level; the width comes from the header. Decodes
 * validate every entry at the declared width (X3/X4/X6) — one bad
 * entry rejects the decode — and accept any width the typed read
 * projects exactly: reading level-N records at level M <= N is
 * exact, appended fields simply read as zero when absent (X1/X12).
 * Version-1 files remain readable (v2-aware readers take both). */
int hot_store_v2_encode_ns(uint8_t *buf, size_t cap, const hot_word_ns *recs,
                           uint32_t count, uint8_t be, size_t *out);
int hot_store_v2_decode_words(uint8_t *buf, size_t cap, hot_time_t *words,
                              uint32_t count_max, uint32_t *count);
int hot_store_v2_decode_ns(uint8_t *buf, size_t cap, hot_word_ns *recs,
                           uint32_t count_max, uint32_t *count);

#endif

#if _HOT_TIME_BITS >= 2

int hot_store_v2_encode_as(uint8_t *buf, size_t cap, const hot_word_as *recs,
                           uint32_t count, uint8_t be, size_t *out);
int hot_store_v2_decode_as(uint8_t *buf, size_t cap, hot_word_as *recs,
                           uint32_t count_max, uint32_t *count);

#endif

#endif
