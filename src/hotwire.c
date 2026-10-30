#include "hot/wire.h"

#include <string.h>

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)(p[1] << 8));
}

static void put_le64(uint8_t *p, uint64_t v)
{
    for (int j = 0; j < 8; j++)
        p[j] = (uint8_t)(v >> (8 * j));
}

static uint64_t get_le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int j = 0; j < 8; j++)
        v |= (uint64_t)p[j] << (8 * j);
    return v;
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#if _HOT_TIME_BITS >= 1
static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void put_be64(uint8_t *p, uint64_t v)
{
    for (int j = 0; j < 8; j++)
        p[j] = (uint8_t)(v >> (8 * (7 - j)));
}

static uint64_t get_be64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int j = 0; j < 8; j++)
        v |= (uint64_t)p[j] << (8 * (7 - j));
    return v;
}
#endif /* _HOT_TIME_BITS >= 1 */

uint8_t hot_wire_negotiate(uint8_t a_min, uint8_t a_max,
                           uint8_t b_min, uint8_t b_max)
{
    if (a_min == 0 || b_min == 0)
        return HOT_WIRE_V_NONE;
    if (a_min > a_max || b_min > b_max)
        return HOT_WIRE_V_NONE;
    uint8_t v = (a_max < b_max) ? a_max : b_max;
    if (v < a_min || v < b_min)
        return HOT_WIRE_V_NONE;
    return v;
}

int hot_wire_hello_encode(uint8_t *buf, size_t cap, uint8_t vmin, uint8_t vmax,
                         size_t *out)
{
    if (buf == NULL)
        return HOT_EMALFORMED;
    if (vmin == 0 || vmin > vmax)
        return HOT_EMALFORMED;
    if (cap < HOT_WIRE_HELLO_SZ)
        return HOT_ENOSPC;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'I';
    buf[2] = vmin;
    buf[3] = vmax;
    if (out != NULL)
        *out = HOT_WIRE_HELLO_SZ;
    return 0;
}

int hot_wire_hello_decode(const uint8_t *buf, size_t cap,
                          uint8_t *vmin, uint8_t *vmax)
{
    if (buf == NULL || cap != HOT_WIRE_HELLO_SZ)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'I')
        return HOT_EMALFORMED;
    if (buf[2] == 0 || buf[2] > buf[3])
        return HOT_EMALFORMED;
    if (vmin != NULL)
        *vmin = buf[2];
    if (vmax != NULL)
        *vmax = buf[3];
    return 0;
}

int hot_wire_v1_encode(uint8_t *buf, size_t cap, int64_t t64,
                       const uint8_t *payload, uint16_t len, size_t *out)
{
    int32_t t32 = 0;

    if (buf == NULL || (len > 0 && payload == NULL))
        return HOT_EMALFORMED;
    int rc = hot_to_32(t64, HOT_ERROR, &t32);
    if (rc != 0)
        return rc;
    if (cap < HOT_WIRE_HDR_V1 + (size_t)len)
        return HOT_ENOSPC;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'1';
    put_le16(buf + 2, len);
    put_le32(buf + 4, (uint32_t)t32);
    if (len > 0)
        memcpy(buf + HOT_WIRE_HDR_V1, payload, (size_t)len);
    if (out != NULL)
        *out = HOT_WIRE_HDR_V1 + (size_t)len;
    return 0;
}

int hot_wire_v1_decode(const uint8_t *buf, size_t cap, uint32_t era,
                       hot_time_t *word, const uint8_t **payload,
                       uint16_t *len)
{
    if (buf == NULL || cap < HOT_WIRE_HDR_V1)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'1')
        return HOT_EMALFORMED;
    uint16_t l = get_le16(buf + 2);
    if (cap != HOT_WIRE_HDR_V1 + (size_t)l)
        return HOT_EMALFORMED;
    int32_t low = (int32_t)get_le32(buf + 4);
    if (word != NULL)
        *word = hot_from_32(low, era);
    if (payload != NULL)
        *payload = buf + HOT_WIRE_HDR_V1;
    if (len != NULL)
        *len = l;
    return 0;
}

int hot_wire_v2_encode(uint8_t *buf, size_t cap, hot_time_t word,
                       const uint8_t *payload, uint16_t len, size_t *out)
{
    if (buf == NULL || (len > 0 && payload == NULL))
        return HOT_EMALFORMED;
    if (cap < HOT_WIRE_HDR_V2 + (size_t)len)
        return HOT_ENOSPC;
    uint8_t flags = hot_is_compat(word) ? HOT_WIRE_F_COMPAT : 0;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'2';
    put_le16(buf + 2, len);
    buf[4] = flags;
    put_le64(buf + 5, word);
    if (len > 0)
        memcpy(buf + HOT_WIRE_HDR_V2, payload, (size_t)len);
    if (out != NULL)
        *out = HOT_WIRE_HDR_V2 + (size_t)len;
    return 0;
}

int hot_wire_v2_decode(const uint8_t *buf, size_t cap, hot_time_t *word,
                       const uint8_t **payload, uint16_t *len)
{
    if (buf == NULL || cap < HOT_WIRE_HDR_V2)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'2')
        return HOT_EMALFORMED;
    uint16_t l = get_le16(buf + 2);
    if (cap != HOT_WIRE_HDR_V2 + (size_t)l)
        return HOT_EMALFORMED;
    uint8_t flags = buf[4];
    if ((flags & HOT_WIRE_F_RESERVED) != 0)
        return HOT_EMALFORMED;
    hot_time_t w = get_le64(buf + 5);
    if (((flags & HOT_WIRE_F_COMPAT) != 0) != hot_is_compat(w))
        return HOT_EMALFORMED;
    if (word != NULL)
        *word = w;
    if (payload != NULL)
        *payload = buf + HOT_WIRE_HDR_V2;
    if (len != NULL)
        *len = l;
    return 0;
}

/* --- v3: CAPS + fraction records (docs/08) --- */

int hot_wire_caps_encode(uint8_t *buf, size_t cap, uint8_t level,
                         uint8_t be, size_t *out)
{
    if (buf == NULL || level > 2 || be > 1)
        return HOT_EMALFORMED;
    if (cap < HOT_WIRE_CAPS_SZ)
        return HOT_ENOSPC;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'C';
    buf[2] = level;
    buf[3] = be;
    if (out != NULL)
        *out = HOT_WIRE_CAPS_SZ;
    return 0;
}

int hot_wire_caps_decode(const uint8_t *buf, size_t cap,
                         uint8_t *level, uint8_t *be)
{
    if (buf == NULL || cap != HOT_WIRE_CAPS_SZ)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'C')
        return HOT_EMALFORMED;
    if (buf[2] > 2 || buf[3] > 1)
        return HOT_EMALFORMED;
    if (level != NULL)
        *level = buf[2];
    if (be != NULL)
        *be = buf[3];
    return 0;
}

#if _HOT_TIME_BITS >= 1

static size_t v3_width(uint8_t level)
{
    return (size_t)8 + (size_t)4 * (size_t)level;
}

static uint8_t v3_level_bits(uint8_t level)
{
    switch (level) {
    case 0:  return 0;
    case 1:  return HOT_WIRE_F_NS;
    default: return (uint8_t)(HOT_WIRE_F_NS | HOT_WIRE_F_AS);
    }
}

int hot_wire_v3_encode(uint8_t *buf, size_t cap, const hot_word_ns *rec,
                       uint8_t level, uint8_t be, const uint8_t *payload,
                       uint16_t len, size_t *out)
{
    hot_word_ns vet;
    size_t w;

    if (buf == NULL || rec == NULL || (len > 0 && payload == NULL))
        return HOT_EMALFORMED;
    if (level > 1 || be > 1)
        return HOT_EMALFORMED;
    if (hot_word_ns_unpack(rec->w, rec->ns, &vet) != 0)
        return HOT_EMALFORMED;              /* X3/X4/X6 */
    if (level == 0 && rec->ns != 0)
        return HOT_ERANGE;                  /* X12 fraction, X7 class */
    w = v3_width(level);
    if (cap < HOT_WIRE_HDR_V3 + w + (size_t)len)
        return HOT_ENOSPC;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'3';
    put_le16(buf + 2, len);                 /* always LE (X9) */
    buf[4] = (uint8_t)(v3_level_bits(level)
                       | (be != 0 ? HOT_WIRE_F_BE : 0)
                       | (hot_is_compat(rec->w) ? HOT_WIRE_F_COMPAT : 0));
    if (be != 0) {
        put_be64(buf + HOT_WIRE_HDR_V3, rec->w);
        if (level == 1)
            put_be32(buf + HOT_WIRE_HDR_V3 + 8, rec->ns);
    } else {
        put_le64(buf + HOT_WIRE_HDR_V3, rec->w);
        if (level == 1)
            put_le32(buf + HOT_WIRE_HDR_V3 + 8, rec->ns);
    }
    if (len > 0)
        memcpy(buf + HOT_WIRE_HDR_V3 + w, payload, (size_t)len);
    if (out != NULL)
        *out = HOT_WIRE_HDR_V3 + w + (size_t)len;
    return 0;
}

int hot_wire_v3_decode(const uint8_t *buf, size_t cap, uint8_t level,
                       uint8_t be, hot_word_ns *rec,
                       const uint8_t **payload, uint16_t *len)
{
    size_t w;
    uint16_t l;
    uint8_t flags;
    uint64_t word;
    uint32_t ns_raw = 0;

    if (buf == NULL || rec == NULL)
        return HOT_EMALFORMED;
    if (level > 1 || be > 1)
        return HOT_EMALFORMED;
    w = v3_width(level);
    if (cap < HOT_WIRE_HDR_V3 + w)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'3')
        return HOT_EMALFORMED;
    l = get_le16(buf + 2);                  /* always LE (X9) */
    flags = buf[4];
    if ((flags & HOT_WIRE_F_RESERVED_V3) != 0)
        return HOT_EMALFORMED;
    if ((flags & (HOT_WIRE_F_NS | HOT_WIRE_F_AS)) != v3_level_bits(level))
        return HOT_EMALFORMED;              /* frame/session mismatch */
    if ((flags & HOT_WIRE_F_BE) != (be != 0 ? HOT_WIRE_F_BE : 0))
        return HOT_EMALFORMED;              /* frame/session mismatch */
    if (cap != HOT_WIRE_HDR_V3 + w + (size_t)l)
        return HOT_EMALFORMED;
    word = (be != 0) ? get_be64(buf + HOT_WIRE_HDR_V3)
                     : get_le64(buf + HOT_WIRE_HDR_V3);
    if (level == 1)
        ns_raw = (be != 0) ? get_be32(buf + HOT_WIRE_HDR_V3 + 8)
                           : get_le32(buf + HOT_WIRE_HDR_V3 + 8);
    if (((flags & HOT_WIRE_F_COMPAT) != 0) != hot_is_compat(word))
        return HOT_EMALFORMED;
    if (hot_word_ns_unpack(word, ns_raw, rec) != 0)
        return HOT_EMALFORMED;              /* X3/X4/X6 */
    if (payload != NULL)
        *payload = buf + HOT_WIRE_HDR_V3 + w;
    if (len != NULL)
        *len = l;
    return 0;
}

#endif /* _HOT_TIME_BITS >= 1 */

#if _HOT_TIME_BITS >= 2

int hot_wire_v3_encode_as(uint8_t *buf, size_t cap, const hot_word_as *rec,
                          uint8_t level, uint8_t be, const uint8_t *payload,
                          uint16_t len, size_t *out)
{
    hot_word_as vet;
    size_t w;

    if (buf == NULL || rec == NULL || (len > 0 && payload == NULL))
        return HOT_EMALFORMED;
    if (level > 2 || be > 1)
        return HOT_EMALFORMED;
    if (hot_word_as_unpack(rec->w, rec->ns, rec->as, &vet) != 0)
        return HOT_EMALFORMED;              /* X3/X4/X6 */
    if (level == 0 && (rec->ns != 0 || rec->as != 0))
        return HOT_ERANGE;                  /* X12 fraction, X7 class */
    if (level == 1 && rec->as != 0)
        return HOT_ERANGE;                  /* X12 */
    w = v3_width(level);
    if (cap < HOT_WIRE_HDR_V3 + w + (size_t)len)
        return HOT_ENOSPC;
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'3';
    put_le16(buf + 2, len);                 /* always LE (X9) */
    buf[4] = (uint8_t)(v3_level_bits(level)
                       | (be != 0 ? HOT_WIRE_F_BE : 0)
                       | (hot_is_compat(rec->w) ? HOT_WIRE_F_COMPAT : 0));
    if (be != 0) {
        put_be64(buf + HOT_WIRE_HDR_V3, rec->w);
        if (level >= 1)
            put_be32(buf + HOT_WIRE_HDR_V3 + 8, rec->ns);
        if (level == 2)
            put_be32(buf + HOT_WIRE_HDR_V3 + 12, (uint32_t)rec->as);
    } else {
        put_le64(buf + HOT_WIRE_HDR_V3, rec->w);
        if (level >= 1)
            put_le32(buf + HOT_WIRE_HDR_V3 + 8, rec->ns);
        if (level == 2)
            put_le32(buf + HOT_WIRE_HDR_V3 + 12, (uint32_t)rec->as);
    }
    if (len > 0)
        memcpy(buf + HOT_WIRE_HDR_V3 + w, payload, (size_t)len);
    if (out != NULL)
        *out = HOT_WIRE_HDR_V3 + w + (size_t)len;
    return 0;
}

int hot_wire_v3_decode_as(const uint8_t *buf, size_t cap, uint8_t level,
                          uint8_t be, hot_word_as *rec,
                          const uint8_t **payload, uint16_t *len)
{
    size_t w;
    uint16_t l;
    uint8_t flags;
    uint64_t word;
    uint32_t ns_raw = 0;
    uint64_t as = 0;

    if (buf == NULL || rec == NULL)
        return HOT_EMALFORMED;
    if (level > 2 || be > 1)
        return HOT_EMALFORMED;
    w = v3_width(level);
    if (cap < HOT_WIRE_HDR_V3 + w)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'3')
        return HOT_EMALFORMED;
    l = get_le16(buf + 2);                  /* always LE (X9) */
    flags = buf[4];
    if ((flags & HOT_WIRE_F_RESERVED_V3) != 0)
        return HOT_EMALFORMED;
    if ((flags & (HOT_WIRE_F_NS | HOT_WIRE_F_AS)) != v3_level_bits(level))
        return HOT_EMALFORMED;              /* frame/session mismatch */
    if ((flags & HOT_WIRE_F_BE) != (be != 0 ? HOT_WIRE_F_BE : 0))
        return HOT_EMALFORMED;              /* frame/session mismatch */
    if (cap != HOT_WIRE_HDR_V3 + w + (size_t)l)
        return HOT_EMALFORMED;
    word = (be != 0) ? get_be64(buf + HOT_WIRE_HDR_V3)
                     : get_le64(buf + HOT_WIRE_HDR_V3);
    if (level >= 1)
        ns_raw = (be != 0) ? get_be32(buf + HOT_WIRE_HDR_V3 + 8)
                           : get_le32(buf + HOT_WIRE_HDR_V3 + 8);
    if (level == 2)
        as = (be != 0) ? (uint64_t)get_be32(buf + HOT_WIRE_HDR_V3 + 12)
                       : (uint64_t)get_le32(buf + HOT_WIRE_HDR_V3 + 12);
    if (((flags & HOT_WIRE_F_COMPAT) != 0) != hot_is_compat(word))
        return HOT_EMALFORMED;
    if (hot_word_as_unpack(word, ns_raw, as, rec) != 0)
        return HOT_EMALFORMED;              /* X3/X4/X6 */
    if (payload != NULL)
        *payload = buf + HOT_WIRE_HDR_V3 + w;
    if (len != NULL)
        *len = l;
    return 0;
}

#endif /* _HOT_TIME_BITS >= 2 */