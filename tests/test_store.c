#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hot/ext.h"
#include "hot/store.h"
#include "hot/time.h"

static int fails = 0;
static int checks = 0;

static void check(const char *what, int cond)
{
    checks++;
    if (cond)
        printf("ok   %s\n", what);
    else {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void check_rc(const char *what, int rc, int want)
{
    checks++;
    if (rc == want)
        printf("ok   %s -> %d\n", what, rc);
    else {
        printf("FAIL %s -> %d want %d\n", what, rc, want);
        fails++;
    }
}

static void check_bytes(const char *what, const uint8_t *got,
                        const uint8_t *want, size_t n)
{
    checks++;
    if (memcmp(got, want, n) == 0) {
        printf("ok   %s (%zu bytes golden)\n", what, n);
    } else {
        printf("FAIL %s\n got:", what);
        for (size_t i = 0; i < n; i++)
            printf(" %02x", got[i]);
        printf("\nwant:");
        for (size_t i = 0; i < n; i++)
            printf(" %02x", want[i]);
        printf("\n");
        fails++;
    }
}

int main(void)
{
    uint8_t buf[64];
    size_t n = 0;
    uint32_t count = 0;
    hot_time_t words[4] = { 0 };

    puts("== encode / decode round-trip + golden bytes (D5/E3/G2) ==");
    {
        hot_time_t in[2];
        in[0] = hot_from_64(INT64_C(1234567890));
        in[1] = hot_from_32(INT32_MIN, 1);
        check_rc("store encode 2 words",
                 hot_store_encode(buf, sizeof buf, in, 2, &n), 0);
        check("store size 12+16", n == 28);
        {
            static const uint8_t want[28] = {
                0x48, 0x4f, 0x54, 0x53,
                0x01, 0x01, 0x00, 0x08,
                0x02, 0x00, 0x00, 0x00,
                0xd2, 0x02, 0x96, 0x49, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80
            };
            check_bytes("store golden bytes", buf, want, 28);
        }
        check_rc("store decode",
                 hot_store_decode(buf, 28, words, 4, &count), 0);
        check("store count", count == 2);
        check("store round-trip",
              words[0] == in[0] && words[1] == in[1]);
    }
    {
        check_rc("store encode 0 words",
                 hot_store_encode(buf, sizeof buf, NULL, 0, &n), 0);
        check("store empty size 12", n == 12);
        check_rc("store decode empty",
                 hot_store_decode(buf, 12, NULL, 0, &count), 0);
        check("store empty count", count == 0);
    }

    puts("== header validation: loud rejection (R6) ==");
    {
        hot_time_t in[1];
        in[0] = hot_from_64(1);
        check_rc("setup one-word store",
                 hot_store_encode(buf, sizeof buf, in, 1, &n), 0);
        check("store size 20", n == 20);
    }
    {
        buf[0] = 'X';
        check_rc("store decode bad magic",
                 hot_store_decode(buf, 20, words, 4, NULL), HOT_EMALFORMED);
        buf[0] = 'H';
    }
    {
        buf[4] = 2;
        check_rc("store decode wrong version",
                 hot_store_decode(buf, 20, words, 4, NULL), HOT_EMALFORMED);
        buf[4] = 1;
    }
    {
        buf[5] = 0x00;
        check_rc("store decode wrong flags",
                 hot_store_decode(buf, 20, words, 4, NULL), HOT_EMALFORMED);
        buf[5] = 0x01;
    }
    {
        buf[6] = 1;
        check_rc("store decode wrong unit",
                 hot_store_decode(buf, 20, words, 4, NULL), HOT_EMALFORMED);
        buf[6] = 0x00;
    }
    {
        buf[7] = 4;
        check_rc("store decode wrong width",
                 hot_store_decode(buf, 20, words, 4, NULL), HOT_EMALFORMED);
        buf[7] = 0x08;
    }
    check_rc("store decode truncated",
             hot_store_decode(buf, 19, words, 4, NULL), HOT_EMALFORMED);
    check_rc("store decode trailing byte",
             hot_store_decode(buf, 21, words, 4, NULL), HOT_EMALFORMED);
    check_rc("store decode count_max too small",
             hot_store_decode(buf, 20, words, 0, NULL), HOT_ENOSPC);

    puts("== legacy32 state files: era-bounded conversion (R4) ==");
    {
        static const int32_t stamps[2] = { INT32_C(0x76543210),
                                           INT32_C(-2) };
        hot_time_t out[2] = { 0 };
        check_rc("legacy32 bounded [INT32_MIN, INT32_MAX]",
                 hot_store_legacy32(stamps, 2, INT64_C(-2147483648),
                                   INT64_C(2147483647), out),
                 0);
        check("legacy32 entry 0 -> era 0 tagged",
              out[0] == hot_from_32(INT32_C(0x76543210), 0));
        check("legacy32 entry 1 -> era 0 tagged (pre-1970)",
              out[1] == hot_from_32(INT32_C(-2), 0));
        check("legacy32 entry 1 seconds -2", hot_seconds(out[1]) == -2);
    }
    {
        static const int32_t stamps[1] = { INT32_C(0x76543210) };
        hot_time_t out[1] = { 0 };
        check_rc("legacy32 bounds spanning eras -> -HOT_EAMBIGUOUS",
                 hot_store_legacy32(stamps, 1, 0, INT64_C(12884901888), out),
                 HOT_EAMBIGUOUS);
        check_rc("legacy32 empty bounds -> -HOT_EAMBIGUOUS",
                 hot_store_legacy32(stamps, 1, 5, 10, out), HOT_EAMBIGUOUS);
        check_rc("legacy32 late bounds uniquely select era 2",
                 hot_store_legacy32(stamps, 1, INT64_C(9147483648),
                                   INT64_C(10747903520), out),
                 0);
        check("legacy32 era-2 word",
              out[0] == hot_from_32(INT32_C(0x76543210), 2));
        check("legacy32 era-2 seconds",
              hot_seconds(out[0]) == INT64_C(10575163920));
    }
    {
        static const int32_t stamps[3] = { INT32_C(100), INT32_C(-50),
                                           INT32_C(0x76543210) };
        hot_time_t out[3] = { 0 };
        check_rc("legacy32 mixed signs, tight bounds -> all convert",
                 hot_store_legacy32(stamps, 3, INT64_C(-2147483648),
                                    INT64_C(2147483647), out),
                 0);
        check("legacy32 mixed words",
              out[0] == hot_from_32(INT32_C(100), 0)
                  && out[1] == hot_from_32(INT32_C(-50), 0)
                  && out[2] == hot_from_32(INT32_C(0x76543210), 0));
        check_rc("legacy32 spanning bounds -> stops loudly at entry 0",
                 hot_store_legacy32(stamps, 3, 0, INT64_C(12884901888), out),
                 HOT_EAMBIGUOUS);
    }

    puts("== storage v2: golden bytes, BE deep-past x2 (docs/08) ==");
    {
        hot_word_ns rec;
        hot_word_ns recs[2];
        static const uint8_t want[36] = {
            0x48, 0x4f, 0x54, 0x53,
            0x02, 0x0b, 0x00, 0x0c,
            0x02, 0x00, 0x00, 0x00,               /* count stays LE (X9) */
            0x3f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
            0x6c, 0xb4, 0x17, 0x80,
            0x3f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
            0x6c, 0xb4, 0x17, 0x80
        };

        check_rc("deep(1901-12-13 20:45:52.75)",
                 hot_word_ns_deep(INT64_C(-2147483648),
                                  UINT32_C(750000000), &rec), 0);
        check("worked example deep word 0x3FFFFFFF80000000",
              rec.w == UINT64_C(0x3FFFFFFF80000000));
        check("worked example deep ns raw 0x6CB41780",
              rec.ns == UINT32_C(0x6CB41780));
        recs[0] = rec;
        recs[1] = rec;
        check_rc("v2 encode ns BE x2",
                 hot_store_v2_encode_ns(buf, sizeof buf, recs, 2, 1, &n), 0);
        check("v2 golden size 36", n == 36);
        check_bytes("v2 golden bytes (flags=0B, width=0C, count LE)",
                    buf, want, 36);
    }

    puts("== storage v2: round-trips, widths 8/12/16, LE + BE (T6) ==");
    {
        hot_time_t in[3];
        hot_word_ns nin[3];
        hot_word_ns nout[3];
        hot_word_as ain[2];
        hot_word_as aout[2];

        in[0] = hot_from_64(INT64_C(1234567890));
        in[1] = hot_from_32(INT32_MIN, 1);
        in[2] = hot_from_32(INT32_C(-1), 0);
        check_rc("v2 encode words LE",
                 hot_store_v2_encode_words(buf, sizeof buf, in, 3, 0, &n), 0);
        check("v2 words size 12+24", n == 36);
        check_rc("v2 decode words LE",
                 hot_store_v2_decode_words(buf, 36, words, 3, &count), 0);
        check("v2 words round-trip LE",
              count == 3 && words[0] == in[0] && words[1] == in[1]
                  && words[2] == in[2]);
        check_rc("v2 encode words BE",
                 hot_store_v2_encode_words(buf, sizeof buf, in, 3, 1, &n), 0);
        check("v2 words BE flips byte order (X9)",
              buf[12] == 0x00 && buf[19] == 0xD2);
        check_rc("v2 decode words BE",
                 hot_store_v2_decode_words(buf, 36, words, 3, &count), 0);
        check("v2 words round-trip BE",
              count == 3 && words[0] == in[0] && words[1] == in[1]
                  && words[2] == in[2]);

        check_rc("plain(2147483647, 5e8)",
                 hot_word_ns_plain(INT64_C(2147483647),
                                   UINT32_C(500000000), &nin[0]), 0);
        check_rc("adopt tagged era-1 word + 2.5e8",
                 hot_word_ns_adopt(hot_from_32(INT32_MIN, 1),
                                   UINT32_C(250000000), &nin[1]), 0);
        check_rc("deep(-2147483648, 7.5e8)",
                 hot_word_ns_deep(INT64_C(-2147483648),
                                  UINT32_C(750000000), &nin[2]), 0);
        check_rc("v2 encode ns LE, three classes",
                 hot_store_v2_encode_ns(buf, sizeof buf, nin, 3, 0, &n), 0);
        check("v2 ns size 12+36", n == 48);
        check_rc("v2 decode ns LE",
                 hot_store_v2_decode_ns(buf, 48, nout, 3, &count), 0);
        check("v2 ns round-trip LE",
              count == 3 && nout[0].w == nin[0].w && nout[0].ns == nin[0].ns
                  && nout[1].w == nin[1].w && nout[1].ns == nin[1].ns
                  && nout[2].w == nin[2].w && nout[2].ns == nin[2].ns);
        check_rc("v2 encode ns BE, three classes",
                 hot_store_v2_encode_ns(buf, sizeof buf, nin, 3, 1, &n), 0);
        check_rc("v2 decode ns BE",
                 hot_store_v2_decode_ns(buf, 48, nout, 3, &count), 0);
        check("v2 ns round-trip BE",
              count == 3 && nout[0].w == nin[0].w && nout[0].ns == nin[0].ns
                  && nout[1].w == nin[1].w && nout[1].ns == nin[1].ns
                  && nout[2].w == nin[2].w && nout[2].ns == nin[2].ns);
        check_rc("v2 decode words of width-12 file (X12 exact)",
                 hot_store_v2_decode_words(buf, 48, words, 3, &count), 0);
        check("v2 words view of ns file",
              count == 3 && words[0] == nin[0].w && words[1] == nin[1].w
                  && words[2] == nin[2].w);

        check_rc("as_deep(-2147483648, 7.5e8, 123456789)",
                 hot_word_as_deep(INT64_C(-2147483648), UINT32_C(750000000),
                                  UINT64_C(123456789), &ain[0]), 0);
        check_rc("as_plain(1, 2, 3)",
                 hot_word_as_plain(INT64_C(1), UINT32_C(2), UINT64_C(3),
                                   &ain[1]), 0);
        check_rc("v2 encode as LE",
                 hot_store_v2_encode_as(buf, sizeof buf, ain, 2, 0, &n), 0);
        check("v2 as size 12+32", n == 44);
        check_rc("v2 decode as LE",
                 hot_store_v2_decode_as(buf, 44, aout, 2, &count), 0);
        check("v2 as round-trip LE",
              count == 2 && aout[0].w == ain[0].w && aout[0].ns == ain[0].ns
                  && aout[0].as == ain[0].as && aout[1].w == ain[1].w
                  && aout[1].ns == ain[1].ns && aout[1].as == ain[1].as);
        check_rc("v2 encode as BE",
                 hot_store_v2_encode_as(buf, sizeof buf, ain, 2, 1, &n), 0);
        check_rc("v2 decode as BE",
                 hot_store_v2_decode_as(buf, 44, aout, 2, &count), 0);
        check("v2 as round-trip BE",
              count == 2 && aout[0].w == ain[0].w && aout[0].ns == ain[0].ns
                  && aout[0].as == ain[0].as && aout[1].w == ain[1].w
                  && aout[1].ns == ain[1].ns && aout[1].as == ain[1].as);
        check_rc("v2 decode ns of width-16 file (X12 exact)",
                 hot_store_v2_decode_ns(buf, 44, nout, 2, &count), 0);
        check("v2 ns view of as file, as not read",
              count == 2 && nout[0].w == ain[0].w && nout[0].ns == ain[0].ns
                  && nout[1].w == ain[1].w && nout[1].ns == ain[1].ns);
    }

    puts("== v1/v2 interop: readers never removed (docs/05) ==");
    {
        hot_time_t in[1];
        hot_word_ns out[1];

        in[0] = hot_from_64(INT64_C(1234567890));
        check_rc("v2 encode one word",
                 hot_store_v2_encode_words(buf, sizeof buf, in, 1, 0, &n), 0);
        check("v2 one-word size 20", n == 20);
        check_rc("v1 reader rejects v2 loudly",
                 hot_store_decode(buf, 20, words, 1, NULL), HOT_EMALFORMED);
        check_rc("v1 encode one word",
                 hot_store_encode(buf, sizeof buf, in, 1, &n), 0);
        check_rc("v2-aware reader takes v1",
                 hot_store_v2_decode_ns(buf, 20, out, 1, &count), 0);
        check("v1 file read at level 1: word exact, ns = 0 (X1/X12)",
              count == 1 && out[0].w == in[0] && out[0].ns == 0);
    }

    puts("== storage v2: loud rejection (X10) ==");
    {
        hot_word_ns in[2];
        hot_word_ns out[2];
        hot_word_as asrec;
        hot_word_as asout[1];

        check_rc("setup plain ns record",
                 hot_word_ns_plain(INT64_C(1), UINT32_C(500000000), &in[0]), 0);
        check_rc("setup deep ns record",
                 hot_word_ns_deep(INT64_C(-2147483648), UINT32_C(750000000),
                                  &in[1]), 0);
        check_rc("v2 encode ns LE x2",
                 hot_store_v2_encode_ns(buf, sizeof buf, in, 2, 0, &n), 0);
        check("v2 two-record size 36", n == 36);
        check_rc("v2 decode good baseline",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count), 0);
        /* entry 1 ns raw field lives at offset 12+12+8 = 32 */
        buf[32] = 0x00; buf[33] = 0xCA; buf[34] = 0x9A; buf[35] = 0x3B;
        check_rc("v2 decode ns = 1e9 (X3), good entry 0 -> whole decode rejects",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[32] = 0x01; buf[33] = 0x00; buf[34] = 0x00; buf[35] = 0x80;
        check_rc("v2 decode class 10 (X4) -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[32] = 0x80; buf[33] = 0x17; buf[34] = 0xB4; buf[35] = 0x6C;
        buf[31] = 0xBF;                        /* deep word gains bit 63 */
        check_rc("v2 decode deep + compat bit (X6) -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[31] = 0x3F;
        buf[5] = 0x02;
        check_rc("v2 decode words bit clear -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[5] = 0x05;
        check_rc("v2 decode as without ns -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[5] = 0x13;
        check_rc("v2 decode reserved flag bits (mask F0) -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[5] = 0x03;
        buf[7] = 0x08;
        check_rc("v2 decode width 8 vs ns flag -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[7] = 0x10;
        check_rc("v2 decode width 16 vs ns-only flag -> -HOT_EMALFORMED",
                 hot_store_v2_decode_ns(buf, 36, out, 2, &count),
                 HOT_EMALFORMED);
        buf[7] = 0x0C;
        check_rc("v2 decode truncated",
                 hot_store_v2_decode_ns(buf, 35, out, 2, &count),
                 HOT_EMALFORMED);
        check_rc("v2 decode trailing byte",
                 hot_store_v2_decode_ns(buf, 37, out, 2, &count),
                 HOT_EMALFORMED);
        check_rc("v2 decode count_max too small",
                 hot_store_v2_decode_ns(buf, 36, out, 1, &count), HOT_ENOSPC);
        check_rc("v2 encode be = 2 -> -HOT_EMALFORMED",
                 hot_store_v2_encode_ns(buf, sizeof buf, in, 2, 2, &n),
                 HOT_EMALFORMED);
        {
            hot_word_ns bad;

            bad.w = UINT64_C(1);
            bad.ns = UINT32_C(1000000000);     /* fraction 1e9: X3 */
            check_rc("v2 encode ns = 1e9 (X3 on send) -> -HOT_EMALFORMED",
                     hot_store_v2_encode_ns(buf, sizeof buf, &bad, 1, 0, &n),
                     HOT_EMALFORMED);
            bad.ns = UINT32_C(0x80000001);     /* class 10: X4 */
            check_rc("v2 encode class 10 (X4 on send) -> -HOT_EMALFORMED",
                     hot_store_v2_encode_ns(buf, sizeof buf, &bad, 1, 0, &n),
                     HOT_EMALFORMED);
            bad.ns = UINT32_C(0x40000000);     /* deep class ... */
            bad.w = UINT64_C(0x8000000180000000); /* ... with compat: X6 */
            check_rc("v2 encode deep + compat (X6 on send) -> -HOT_EMALFORMED",
                     hot_store_v2_encode_ns(buf, sizeof buf, &bad, 1, 0, &n),
                     HOT_EMALFORMED);
        }
        check_rc("setup as record",
                 hot_word_as_deep(INT64_C(-2147483648), UINT32_C(750000000),
                                  UINT64_C(1), &asrec), 0);
        check_rc("v2 encode as LE",
                 hot_store_v2_encode_as(buf, sizeof buf, &asrec, 1, 0, &n), 0);
        check("v2 as one-record size 28", n == 28);
        buf[24] = 0x00; buf[25] = 0xCA; buf[26] = 0x9A; buf[27] = 0x3B;
        check_rc("v2 decode as = 1e9 (X3) -> -HOT_EMALFORMED",
                 hot_store_v2_decode_as(buf, 28, asout, 1, &count),
                 HOT_EMALFORMED);
        {
            hot_word_as bad;

            bad.w = UINT64_C(1);
            bad.ns = UINT32_C(0);
            bad.as = UINT64_C(1000000000);     /* as 1e9: X3 */
            check_rc("v2 encode as = 1e9 (X3 on send) -> -HOT_EMALFORMED",
                     hot_store_v2_encode_as(buf, sizeof buf, &bad, 1, 0, &n),
                     HOT_EMALFORMED);
        }
        check_rc("v2 encode empty ns store",
                 hot_store_v2_encode_ns(buf, sizeof buf, NULL, 0, 0, &n), 0);
        check("v2 empty size 12", n == 12);
        check_rc("v2 decode empty ns store",
                 hot_store_v2_decode_ns(buf, 12, NULL, 0, &count), 0);
        check("v2 empty count 0", count == 0);
    }

    printf("%s: %d failures / %d checks\n",
           fails ? "TEST FAILED" : "TEST PASSED", fails, checks);
    return fails != 0;
}