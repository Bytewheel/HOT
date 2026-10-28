#ifndef HOT_WIRE_H
#define HOT_WIRE_H

#include <stddef.h>
#include <stdint.h>

#include "hot/time.h"

enum hot_wire_ver { HOT_WIRE_V_NONE = 0, HOT_WIRE_V1 = 1, HOT_WIRE_V2 = 2 };

#define HOT_WIRE_F_COMPAT   UINT8_C(0x01)
#define HOT_WIRE_F_RESERVED UINT8_C(0xFE)

#define HOT_WIRE_HDR_V1   ((size_t)8)
#define HOT_WIRE_HDR_V2   ((size_t)13)
#define HOT_WIRE_HELLO_SZ ((size_t)4)

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

#endif