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

static void put_be64(uint8_t *p, uint64_t v)
{
    for (int j = 0; j < 8; j++)
        p[j] = (uint8_t)(v >> (8 * (7 - j)));
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

static uint64_t get_be64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int j = 0; j < 8; j++)
        v |= (uint64_t)p[j] << (8 * (7 - j));
    return v;
}
#endif /* _HOT_TIME_BITS >= 1 */

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
        int rc = hot_from_32_bounded(stamps[i], lower, upper, out + i);
        if (rc != 0)
            return rc;
    }
    return 0;
}

/* --- v2: fraction records + byte order (docs/08) --- */

static void v2_put_header(uint8_t *buf, uint8_t flags, uint8_t width,
                          uint32_t count)
{
    buf[0] = (uint8_t)'H';
    buf[1] = (uint8_t)'O';
    buf[2] = (uint8_t)'T';
    buf[3] = (uint8_t)'S';
    buf[4] = HOT_STORE_VER_V2;
    buf[5] = flags;
    buf[6] = HOT_STORE_UNIT_SEC;
    buf[7] = width;
    put_le32(buf + 8, count);               /* always LE (X9) */
}

#if _HOT_TIME_BITS >= 1
static int v2_parse_header(const uint8_t *buf, size_t cap, uint32_t *n,
                           uint8_t *width, uint8_t *be)
{
    uint8_t base;

    if (buf == NULL || cap < HOT_STORE_HDR)
        return HOT_EMALFORMED;
    if (buf[0] != (uint8_t)'H' || buf[1] != (uint8_t)'O'
        || buf[2] != (uint8_t)'T' || buf[3] != (uint8_t)'S')
        return HOT_EMALFORMED;
    if (buf[6] != HOT_STORE_UNIT_SEC)
        return HOT_EMALFORMED;
    if (buf[4] == HOT_STORE_VER) {
        /* v1 files stay readable: width-8 LE words, exact v1 header */
        if (buf[5] != HOT_STORE_F_WORDS || buf[7] != HOT_STORE_WIDTH)
            return HOT_EMALFORMED;
        *width = HOT_STORE_WIDTH;
        *be = 0;
    } else if (buf[4] == HOT_STORE_VER_V2) {
        uint8_t flags = buf[5];
        if ((flags & HOT_STORE_F_RESERVED_V2) != 0)
            return HOT_EMALFORMED;
        if ((flags & HOT_STORE_F_WORDS) == 0)
            return HOT_EMALFORMED;          /* bit 0 MUST be 1 */
        base = (uint8_t)(flags & (HOT_STORE_F_NS | HOT_STORE_F_AS));
        if (base == HOT_STORE_F_AS)
            return HOT_EMALFORMED;          /* as without ns */
        *width = (base == 0) ? 8 : (base == HOT_STORE_F_NS) ? 12 : 16;
        if (buf[7] != *width)
            return HOT_EMALFORMED;          /* flags/width contradiction */
        *be = (flags & HOT_STORE_F_BE) != 0;
    } else {
        return HOT_EMALFORMED;
    }
    {
        uint32_t cnt = get_le32(buf + 8);   /* always LE (X9) */
        if (cap != HOT_STORE_HDR + (size_t)cnt * (size_t)*width)
            return HOT_EMALFORMED;
        *n = cnt;
    }
    return 0;
}
#endif /* _HOT_TIME_BITS >= 1 */

int hot_store_v2_encode_words(uint8_t *buf, size_t cap, const hot_time_t *words,
                              uint32_t count, uint8_t be, size_t *out)
{
    if (buf == NULL || (count > 0 && words == NULL) || be > 1)
        return HOT_EMALFORMED;
    if (cap < HOT_STORE_HDR + (size_t)count * sizeof(hot_time_t))
        return HOT_ENOSPC;
    v2_put_header(buf,
                  (uint8_t)(HOT_STORE_F_WORDS | (be != 0 ? HOT_STORE_F_BE : 0)),
                  HOT_STORE_WIDTH, count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *p = buf + HOT_STORE_HDR + (size_t)i * sizeof(hot_time_t);
        if (be != 0)
            put_be64(p, words[i]);
        else
            put_le64(p, words[i]);
    }
    if (out != NULL)
        *out = HOT_STORE_HDR + (size_t)count * sizeof(hot_time_t);
    return 0;
}

#if _HOT_TIME_BITS >= 1

/* Read one declared-width entry; absent appended fields read as
 * zero, the exact level-M view of a level-N record (X1/X12). */
static void entry_read(const uint8_t *p, uint8_t width, uint8_t be,
                       uint64_t *w, uint32_t *ns_raw, uint64_t *as)
{
    *w = (be != 0) ? get_be64(p) : get_le64(p);
    if (width >= 12)
        *ns_raw = (be != 0) ? get_be32(p + 8) : get_le32(p + 8);
    else
        *ns_raw = 0;
    if (width == 16)
        *as = (be != 0) ? (uint64_t)get_be32(p + 12)
                        : (uint64_t)get_le32(p + 12);
    else
        *as = 0;
}

/* Validate one entry at its declared width (X3/X4/X6). */
static int entry_valid(uint64_t w, uint32_t ns_raw, uint64_t as, uint8_t width)
{
    hot_word_ns vet;

    if (width == 8)
        return 1;                           /* plain word: any docs/03 word */
    if (hot_word_ns_unpack(w, ns_raw, &vet) != 0)
        return 0;
    if (width == 16 && as >= HOT_AS_MAX)
        return 0;
    return 1;
}

int hot_store_v2_encode_ns(uint8_t *buf, size_t cap, const hot_word_ns *recs,
                           uint32_t count, uint8_t be, size_t *out)
{
    hot_word_ns vet;

    if (buf == NULL || (count > 0 && recs == NULL) || be > 1)
        return HOT_EMALFORMED;
    for (uint32_t i = 0; i < count; i++)
        if (hot_word_ns_unpack(recs[i].w, recs[i].ns, &vet) != 0)
            return HOT_EMALFORMED;          /* X3/X4/X6 */
    if (cap < HOT_STORE_HDR + (size_t)count * 12)
        return HOT_ENOSPC;
    v2_put_header(buf,
                  (uint8_t)(HOT_STORE_F_WORDS | HOT_STORE_F_NS
                            | (be != 0 ? HOT_STORE_F_BE : 0)),
                  12, count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *p = buf + HOT_STORE_HDR + (size_t)i * 12;
        if (be != 0) {
            put_be64(p, recs[i].w);
            put_be32(p + 8, recs[i].ns);
        } else {
            put_le64(p, recs[i].w);
            put_le32(p + 8, recs[i].ns);
        }
    }
    if (out != NULL)
        *out = HOT_STORE_HDR + (size_t)count * 12;
    return 0;
}

int hot_store_v2_decode_words(uint8_t *buf, size_t cap, hot_time_t *words,
                              uint32_t count_max, uint32_t *count)
{
    uint32_t n;
    uint8_t width, be;
    int rc = v2_parse_header(buf, cap, &n, &width, &be);

    if (rc != 0)
        return rc;
    if (n > count_max)
        return HOT_ENOSPC;
    if (n > 0 && words == NULL)
        return HOT_EMALFORMED;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t w, as;
        uint32_t ns_raw;

        entry_read(buf + HOT_STORE_HDR + (size_t)i * width, width, be,
                   &w, &ns_raw, &as);
        if (!entry_valid(w, ns_raw, as, width))
            return HOT_EMALFORMED;
        words[i] = w;
    }
    if (count != NULL)
        *count = n;
    return 0;
}

int hot_store_v2_decode_ns(uint8_t *buf, size_t cap, hot_word_ns *recs,
                           uint32_t count_max, uint32_t *count)
{
    uint32_t n;
    uint8_t width, be;
    int rc = v2_parse_header(buf, cap, &n, &width, &be);

    if (rc != 0)
        return rc;
    if (n > count_max)
        return HOT_ENOSPC;
    if (n > 0 && recs == NULL)
        return HOT_EMALFORMED;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t w, as;
        uint32_t ns_raw;

        entry_read(buf + HOT_STORE_HDR + (size_t)i * width, width, be,
                   &w, &ns_raw, &as);
        if (!entry_valid(w, ns_raw, as, width))
            return HOT_EMALFORMED;
        recs[i].w = w;
        recs[i].ns = (width >= 12) ? ns_raw : 0;
    }
    if (count != NULL)
        *count = n;
    return 0;
}

#endif /* _HOT_TIME_BITS >= 1 */

#if _HOT_TIME_BITS >= 2

int hot_store_v2_encode_as(uint8_t *buf, size_t cap, const hot_word_as *recs,
                           uint32_t count, uint8_t be, size_t *out)
{
    hot_word_as vet;

    if (buf == NULL || (count > 0 && recs == NULL) || be > 1)
        return HOT_EMALFORMED;
    for (uint32_t i = 0; i < count; i++)
        if (hot_word_as_unpack(recs[i].w, recs[i].ns, recs[i].as, &vet) != 0)
            return HOT_EMALFORMED;          /* X3/X4/X6 */
    if (cap < HOT_STORE_HDR + (size_t)count * 16)
        return HOT_ENOSPC;
    v2_put_header(buf,
                  (uint8_t)(HOT_STORE_F_WORDS | HOT_STORE_F_NS
                            | HOT_STORE_F_AS
                            | (be != 0 ? HOT_STORE_F_BE : 0)),
                  16, count);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *p = buf + HOT_STORE_HDR + (size_t)i * 16;
        if (be != 0) {
            put_be64(p, recs[i].w);
            put_be32(p + 8, recs[i].ns);
            put_be32(p + 12, (uint32_t)recs[i].as);
        } else {
            put_le64(p, recs[i].w);
            put_le32(p + 8, recs[i].ns);
            put_le32(p + 12, (uint32_t)recs[i].as);
        }
    }
    if (out != NULL)
        *out = HOT_STORE_HDR + (size_t)count * 16;
    return 0;
}

int hot_store_v2_decode_as(uint8_t *buf, size_t cap, hot_word_as *recs,
                           uint32_t count_max, uint32_t *count)
{
    uint32_t n;
    uint8_t width, be;
    int rc = v2_parse_header(buf, cap, &n, &width, &be);

    if (rc != 0)
        return rc;
    if (n > count_max)
        return HOT_ENOSPC;
    if (n > 0 && recs == NULL)
        return HOT_EMALFORMED;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t w, as;
        uint32_t ns_raw;

        entry_read(buf + HOT_STORE_HDR + (size_t)i * width, width, be,
                   &w, &ns_raw, &as);
        if (!entry_valid(w, ns_raw, as, width))
            return HOT_EMALFORMED;
        recs[i].w = w;
        recs[i].ns = (width >= 12) ? ns_raw : 0;
        recs[i].as = (width == 16) ? as : 0;
    }
    if (count != NULL)
        *count = n;
    return 0;
}

#endif /* _HOT_TIME_BITS >= 2 */