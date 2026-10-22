#include "hot/store.h"

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_le64(uint8_t *p, uint64_t v)
{
    for (int j = 0; j < 8; j++)
        p[j] = (uint8_t)(v >> (8 * j));
}

static uint32_t get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int j = 0; j < 8; j++)
        v |= (uint64_t)p[j] << (8 * j);
    return v;
}

int hot_store_encode(uint8_t *buf, size_t cap, const hot_time_t *words,
                     uint32_t count, size_t *out)
{
    if (buf == NULL || (count > 0 && words == NULL))
        return HOT_EMALFORMED;
    if (cap < HOT_STORE_HDR + (size_t)count * sizeof(hot_time_t))
        return HOT_ENOSPC;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'O';
    buf[2] = (uint8_t)'T';
    buf[3] = (uint8_t)'S';
    buf[4] = HOT_STORE_VER;
    buf[5] = HOT_STORE_F_WORDS;
    buf[6] = HOT_STORE_UNIT_SEC;
    buf[7] = HOT_STORE_WIDTH;
    put_le32(buf + 8, count);
    for (uint32_t i = 0; i < count; i++)
        put_le64(buf + HOT_STORE_HDR + (size_t)i * sizeof(hot_time_t),
                 words[i]);
    if (out != NULL)
        *out = HOT_STORE_HDR + (size_t)count * sizeof(hot_time_t);
    return 0;
}

int hot_store_decode(const uint8_t *buf, size_t cap, hot_time_t *words,
                     uint32_t count_max, uint32_t *count)
{
    if (buf == NULL || cap < HOT_STORE_HDR)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'O'
        || buf[2] != (uint8_t)'T' || buf[3] != (uint8_t)'S')
        return HOT_EMALFORMED;
    if (buf[4] != HOT_STORE_VER)
        return HOT_EMALFORMED;
    if (buf[5] != HOT_STORE_F_WORDS)
        return HOT_EMALFORMED;
    if (buf[6] != HOT_STORE_UNIT_SEC)
        return HOT_EMALFORMED;
    if (buf[7] != HOT_STORE_WIDTH)
        return HOT_EMALFORMED;
    uint32_t n = get_le32(buf + 8);
    if (cap != HOT_STORE_HDR + (size_t)n * sizeof(hot_time_t))
        return HOT_EMALFORMED;
    if (n > count_max)
        return HOT_ENOSPC;
    for (uint32_t i = 0; i < n; i++)
        words[i] = get_le64(buf + HOT_STORE_HDR
                              + (size_t)i * sizeof(hot_time_t));
    if (count != NULL)
        *count = n;
    return 0;
}

int hot_store_legacy32(const int32_t *stamps, uint32_t count,
                       int64_t lower, int64_t upper, hot_time_t *out)
{
    if (count > 0 && (stamps == NULL || out == NULL))
        return HOT_EMALFORMED;
    for (uint32_t i = 0; i < count; i++) {
        int rc = hot_from_32_bounded(stamps[i], lower, upper, &out[i]);
        if (rc != 0)
            return rc;
    }
    return 0;
}