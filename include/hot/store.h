#ifndef HOT_STORE_H
#define HOT_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "hot/time.h"

#define HOT_STORE_VER     UINT8_C(1)
#define HOT_STORE_F_WORDS UINT8_C(0x01)
#define HOT_STORE_UNIT_SEC UINT8_C(0)
#define HOT_STORE_WIDTH   UINT8_C(8)
#define HOT_STORE_HDR     ((size_t)12)

int hot_store_encode(uint8_t *buf, size_t cap, const hot_time_t *words,
                     uint32_t count, size_t *out);
int hot_store_decode(const uint8_t *buf, size_t cap, hot_time_t *words,
                     uint32_t count_max, uint32_t *count);
int hot_store_legacy32(const int32_t *stamps, uint32_t count,
                       int64_t lower, int64_t upper, hot_time_t *out);

#endif