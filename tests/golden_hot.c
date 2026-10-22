#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hot/time.h"

#define GOLDEN_PATH "tests/golden_wire.txt"

struct row {
    const char *desc;
    int kind;
    int64_t a;
    uint32_t b;
};

static const struct row ROWS[] = {
    { "worked-ex-1 from_32(INT32_MAX, 0) 2038-01-19T03:14:07Z", 0,
      INT32_MAX, 0 },
    { "worked-ex-2 from_32(INT32_MIN, 1) 2038-01-19T03:14:08Z", 0,
      INT32_MIN, 1 },
    { "worked-ex-3 from_32(0, 0) 1970-01-01T00:00:00Z", 0, 0, 0 },
    { "worked-ex-4 from_32(INT32_MIN, 0) 1901-12-13T20:45:52Z", 0,
      INT32_MIN, 0 },
    { "from_32(-2, 0) 1969-12-31T23:59:58Z pre-epoch", 0, -2, 0 },
    { "from_32(123456789, 2) era-2 tagged", 0, INT32_C(123456789), 2 },
    { "from_32(1, 3) era-3 tagged", 0, 1, 3 },
    { "from_32(INT32_MIN, 3) era-3 negative-low", 0, INT32_MIN, 3 },
    { "from_u32(0x80000000, 0) -> signed era 1 2038-01-19T03:14:08Z", 1,
      INT64_C(0x80000000), 0 },
    { "from_u32(0xFFFFFFFF, 0) 2106-02-07T06:28:15Z", 1,
      INT64_C(0xFFFFFFFF), 0 },
    { "from_u32(0, 1) 2106-02-07T06:28:16Z", 1, 0, 1 },
    { "from_u32(1234567890, 0) unsigned era 0", 1, INT64_C(1234567890), 0 },
    { "from_64(0) untagged epoch", 2, 0, 0 },
    { "from_64(1234567890) untagged 2009-02-13T23:31:30Z", 2,
      INT64_C(1234567890), 0 },
    { "from_64(INT32_MAX) untagged 2038-01-19T03:14:07Z", 2, INT32_MAX, 0 },
    { "from_64(2147483648) untagged 2038-01-19T03:14:08Z", 2,
      INT64_C(2147483648), 0 },
    { "from_64(4294967296) untagged 2106-02-07T06:28:16Z", 2,
      INT64_C(4294967296), 0 },
    { "from_64(8589934592) untagged 2^33", 2, INT64_C(8589934592), 0 },
};

static hot_time_t row_word(const struct row *r)
{
    switch (r->kind) {
    case 0:  return hot_from_32((int32_t)r->a, r->b);
    case 1:  return hot_from_u32((uint32_t)r->a, r->b);
    default: return hot_from_64(r->a);
    }
}

static int append(char *buf, size_t cap, size_t *n, const char *line)
{
    size_t len = strlen(line);
    if (*n + len >= cap)
        return -1;
    memcpy(buf + *n, line, len);
    *n += len;
    return 0;
}

static int gen(char *buf, size_t cap)
{
    char line[512];
    size_t n = 0;

    if (append(buf, cap, &n,
               "# HOT golden wire fixtures (T7, docs/04-validation.md)\n")
        || append(buf, cap, &n, "# regenerate with: make golden-update\n")
        || append(buf, cap, &n,
               "# desc | word | wire-le | class(u/c) | era | seconds | iso8601\n"))
        return -1;

    for (size_t i = 0; i < sizeof ROWS / sizeof *ROWS; i++) {
        const struct row *r = &ROWS[i];
        hot_time_t word = row_word(r);
        uint8_t wb[8];
        char iso[40];
        time_t tt;
        struct tm *tm;
        int k;

        for (int j = 0; j < 8; j++)
            wb[j] = (uint8_t)(word >> (8 * j));
        tt = (time_t)hot_seconds(word);
        tm = gmtime(&tt);
        if (tm == NULL)
            strcpy(iso, "OVERFLOW");
        else {
            k = snprintf(iso, sizeof iso,
                         "%04d-%02d-%02dT%02d:%02d:%02dZ",
                         tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                         tm->tm_hour, tm->tm_min, tm->tm_sec);
            if (k < 0 || (size_t)k >= sizeof iso)
                return -1;
        }
        k = snprintf(line, sizeof line,
                     "%s | 0x%016" PRIX64 " | "
                     "%02x%02x%02x%02x%02x%02x%02x%02x | "
                     "%c | %" PRIu32 " | %" PRId64 " | %s\n",
                     r->desc, word, wb[0], wb[1], wb[2], wb[3],
                     wb[4], wb[5], wb[6], wb[7],
                     hot_is_compat(word) ? 'c' : 'u', hot_era_of(word),
                     hot_seconds(word), iso);
        if (k < 0 || (size_t)k >= sizeof line)
            return -1;
        if (append(buf, cap, &n, line))
            return -1;
    }
    buf[n] = '\0';
    return (int)n;
}

int main(int argc, char **argv)
{
    static char want[16384], have[16384];
    int n = gen(want, sizeof want);
    FILE *f;
    size_t got;

    if (n < 0) {
        fprintf(stderr, "fixture generation overflow\n");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "--update") == 0) {
        f = fopen(GOLDEN_PATH, "wb");
        if (!f || fwrite(want, 1, (size_t)n, f) != (size_t)n
            || fclose(f) != 0) {
            fprintf(stderr, "cannot write %s\n", GOLDEN_PATH);
            return 1;
        }
        printf("GOLDEN UPDATED: %s (%d bytes)\n", GOLDEN_PATH, n);
        return 0;
    }

    f = fopen(GOLDEN_PATH, "rb");
    if (!f) {
        fprintf(stderr, "missing %s -- run: make golden-update\n",
                GOLDEN_PATH);
        return 1;
    }
    got = fread(have, 1, sizeof have - 1, f);
    fclose(f);
    have[got] = '\0';

    if (got != (size_t)n || memcmp(have, want, (size_t)n) != 0) {
        size_t i = 0;
        while (i < got && i < (size_t)n && have[i] == want[i])
            i++;
        while (i > 0 && have[i - 1] != '\n')
            i--;
        fprintf(stderr,
                "GOLDEN MISMATCH at byte %zu (line starting there)\n"
                "  fixture : %.76s\n  current : %.76s\n"
                "  regenerate with: make golden-update\n",
                i, have + i, want + i);
        return 1;
    }
    printf("GOLDEN PASSED: %zu bytes byte-exact\n", got);
    return 0;
}