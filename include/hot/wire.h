#ifndef HOT_WIRE_H
#define HOT_WIRE_H

#include <stddef.h>
#include <stdint.h>

#include "hot/ext.h"

enum hot_wire_ver {
    HOT_WIRE_V_NONE = 0, HOT_WIRE_V1 = 1, HOT_WIRE_V2 = 2, HOT_WIRE_V3 = 3
};

#define HOT_WIRE_F_COMPAT      UINT8_C(0x01)
#define HOT_WIRE_F_RESERVED    UINT8_C(0xFE)     /* v1/v2 mask (R15) */
#define HOT_WIRE_F_NS          UINT8_C(0x02)     /* v3: ns field present */
#define HOT_WIRE_F_AS          UINT8_C(0x04)     /* v3: as field present */
#define HOT_WIRE_F_BE          UINT8_C(0x08)     /* v3: record is BE */
#define HOT_WIRE_F_RESERVED_V3 UINT8_C(0xF0)     /* v3 mask (docs/08) */

#define HOT_WIRE_HDR_V1   ((size_t)8)
#define HOT_WIRE_HDR_V2   ((size_t)13)
#define HOT_WIRE_HDR_V3   ((size_t)5)            /* + record width W */
#define HOT_WIRE_HELLO_SZ ((size_t)4)
#define HOT_WIRE_CAPS_SZ  ((size_t)4)

uint8_t hot_wire_negotiate(uint8_t a_min, uint8_t a_max,
                           uint8_t b_min, uint8_t b_max);

int hot_wire_hello_encode(uint8_t *buf, size_t cap, uint8_t vmin, uint8_t vmax,
                          size_t *out);
int hot_wire_hello_decode(const uint8_t *buf, size_t cap,
                          uint8_t *vmin, uint8_t *vmax);

int hot_wire_v1_encode(uint8_t *buf, size_t cap, int64_t t64,
                       const uint8_t *payload, uint16_t len, size_t *out);
int hot_wire_v1_decode(const uint8_t *buf, size_t cap, uint32_t era,
                       hot_time_t *word, const uint8_t **payload,
                       uint16_t *len);

int hot_wire_v2_encode(uint8_t *buf, size_t cap, hot_time_t word,
                       const uint8_t *payload, uint16_t len, size_t *out);
int hot_wire_v2_decode(const uint8_t *buf, size_t cap, hot_time_t *word,
                       const uint8_t **payload, uint16_t *len);

/* v3 capability exchange (docs/08): magic "HC", this peer's maximum
 * fraction level (0..2) and big-endian acceptance (0/1). Exchanged
 * once per v3 session right after HELLO; session level = min of the
 * two levels, session BE iff both accept. */
int hot_wire_caps_encode(uint8_t *buf, size_t cap, uint8_t level,
                         uint8_t be, size_t *out);
int hot_wire_caps_decode(const uint8_t *buf, size_t cap,
                         uint8_t *level, uint8_t *be);

#if _HOT_TIME_BITS >= 1

/* v3 data frames at session levels 0-1. level and be are the session
 * parameters from HELLO+CAPS; the frame's flags MUST match them
 * exactly (X12). Level-0 encode requires zero fraction fields (X12)
 * and rejects deep records (X7) with -HOT_ERANGE; level-0 decode
 * yields the exact level-1 view ns = 0. */
int hot_wire_v3_encode(uint8_t *buf, size_t cap, const hot_word_ns *rec,
                       uint8_t level, uint8_t be, const uint8_t *payload,
                       uint16_t len, size_t *out);
int hot_wire_v3_decode(const uint8_t *buf, size_t cap, uint8_t level,
                       uint8_t be, hot_word_ns *rec,
                       const uint8_t **payload, uint16_t *len);

#endif

#if _HOT_TIME_BITS >= 2

/* v3 data frames at session levels 0-2 (hot_word_as records). */
int hot_wire_v3_encode_as(uint8_t *buf, size_t cap, const hot_word_as *rec,
                          uint8_t level, uint8_t be, const uint8_t *payload,
                          uint16_t len, size_t *out);
int hot_wire_v3_decode_as(const uint8_t *buf, size_t cap, uint8_t level,
                          uint8_t be, hot_word_as *rec,
                          const uint8_t **payload, uint16_t *len);

#endif

#endif
