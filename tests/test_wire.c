#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hot/ext.h"
#include "hot/time.h"
#include "hot/wire.h"

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

static void check_ver(const char *what, uint8_t got, uint8_t want)
{
    checks++;
    if (got == want)
        printf("ok   %s -> v%u\n", what, (unsigned)got);
    else {
        printf("FAIL %s -> v%u want v%u\n", what, (unsigned)got,
               (unsigned)want);
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
    hot_time_t word = 0;
    const uint8_t *pl = NULL;
    uint16_t len = 0;

    puts("== negotiation (F1) ==");
    check_ver("neg {1..2} x {1..2} -> v2",
              hot_wire_negotiate(1, 2, 1, 2), 2);
    check_ver("neg {1..2} x {2..2} -> v2",
              hot_wire_negotiate(1, 2, 2, 2), 2);
    check_ver("neg {2..2} x {1..2} -> v2",
              hot_wire_negotiate(2, 2, 1, 2), 2);
    check_ver("neg {1..2} x {1..1} -> v1",
              hot_wire_negotiate(1, 2, 1, 1), 1);
    check_ver("neg {1..1} x {2..2} -> none",
              hot_wire_negotiate(1, 1, 2, 2), 0);
    check_ver("neg {1..2} x {3..4} -> none",
              hot_wire_negotiate(1, 2, 3, 4), 0);
    check_ver("neg {3..4} x {1..2} -> none",
              hot_wire_negotiate(3, 4, 1, 2), 0);
    check_ver("neg {0..2} x {1..2} -> none (vmin 0 invalid)",
              hot_wire_negotiate(0, 2, 1, 2), 0);
    check_ver("neg {2..1} x {1..2} -> none (min>max)",
              hot_wire_negotiate(2, 1, 1, 2), 0);

    puts("== hello ==");
    check_rc("hello encode", hot_wire_hello_encode(buf, sizeof buf, 1, 2, &n),
             0);
    check("hello encoded size 4", n == 4);
    {
        static const uint8_t want[4] = { 0x48, 0x49, 0x01, 0x02 };
        check_bytes("hello golden bytes", buf, want, 4);
    }
    {
        uint8_t lo = 0, hi = 0;
        check_rc("hello decode", hot_wire_hello_decode(buf, 4, &lo, &hi), 0);
        check("hello round-trip", lo == 1 && hi == 2);
    }
    check_rc("hello decode cap 3", hot_wire_hello_decode(buf, 3, NULL, NULL),
             HOT_EMALFORMED);
    check_rc("hello decode cap 5", hot_wire_hello_decode(buf, 5, NULL, NULL),
             HOT_EMALFORMED);
    {
        uint8_t bad[4] = { 0x48, 0x58, 0x01, 0x02 };
        check_rc("hello decode bad magic",
                 hot_wire_hello_decode(bad, 4, NULL, NULL), HOT_EMALFORMED);
    }
    check_rc("hello encode vmin 0",
             hot_wire_hello_encode(buf, sizeof buf, 0, 2, NULL),
             HOT_EMALFORMED);
    check_rc("hello encode min>max",
             hot_wire_hello_encode(buf, sizeof buf, 3, 2, NULL),
             HOT_EMALFORMED);

    puts("== v2 frame: golden bytes + round-trip (G2) ==");
    {
        static const uint8_t payload[2] = { 'A', 'B' };
        check_rc("v2 encode tagged",
                 hot_wire_v2_encode(buf, sizeof buf,
                                     hot_from_32(INT32_MIN, 1), payload, 2,
                                     &n),
                 0);
        check("v2 size 13+2", n == 15);
        {
            static const uint8_t want[15] = {
                0x48, 0x32, 0x02, 0x00, 0x01,
                0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80,
                'A', 'B'
            };
            check_bytes("v2 golden bytes (tagged, flags=01)", buf, want, 15);
        }
        check_rc("v2 decode", hot_wire_v2_decode(buf, 15, &word, &pl, &len),
                 0);
        check("v2 word round-trip",
              word == hot_from_32(INT32_MIN, 1));
        check("v2 payload ptr + len", len == 2 && pl[0] == 'A' && pl[1] == 'B');
    }
    {
        static const uint8_t payload[1] = { 'x' };
        check_rc("v2 encode untagged",
                 hot_wire_v2_encode(buf, sizeof buf, hot_from_64(1234567890),
                                    payload, 1, &n),
                 0);
        {
            static const uint8_t want[14] = {
                0x48, 0x32, 0x01, 0x00, 0x00,
                0xd2, 0x02, 0x96, 0x49, 0x00, 0x00, 0x00, 0x00,
                'x'
            };
            check_bytes("v2 golden bytes (untagged, flags=00)", buf, want, 14);
        }
    }

    puts("== v2 frame: loud rejection (R6) ==");
    check_rc("v2 decode truncated", hot_wire_v2_decode(buf, 12, NULL, NULL,
                                                       NULL),
             HOT_EMALFORMED);
    check_rc("v2 decode trailing byte", hot_wire_v2_decode(buf, 16, NULL, NULL,
                                                          NULL),
             HOT_EMALFORMED);
    {
        buf[4] = 0x03;
        check_rc("v2 decode reserved flags bits",
                 hot_wire_v2_decode(buf, 14, NULL, NULL, NULL),
                 HOT_EMALFORMED);
        buf[4] = 0x02;
        check_rc("v2 decode flags bit 1 alone -> loud error (R15)",
                 hot_wire_v2_decode(buf, 14, NULL, NULL, NULL),
                 HOT_EMALFORMED);
        buf[4] = 0x01;
    }
    {
        check_rc("v2 decode compat-bit mismatch",
                 hot_wire_v2_decode(buf, 14, NULL, NULL, NULL),
                 HOT_EMALFORMED);
        buf[4] = 0x00;
    }
    {
        buf[0] = 'X';
        check_rc("v2 decode bad magic",
                 hot_wire_v2_decode(buf, 15, NULL, NULL, NULL),
                 HOT_EMALFORMED);
        buf[0] = 'H';
    }
    {
        static const uint8_t v1[10] = {
            0x48, 0x31, 0x02, 0x00,
            0xd2, 0x02, 0x96, 0x49,
            'A', 'B'
        };
        check_rc("v2 decoder fed v1 frame -> loud error",
                 hot_wire_v2_decode(v1, 10, NULL, NULL, NULL), HOT_EMALFORMED);
    }

    puts("== v1 frame: legacy, golden bytes + loud refusal (D6/F1/G4) ==");
    {
        static const uint8_t payload[2] = { 'A', 'B' };
        check_rc("v1 encode 1234567890",
                 hot_wire_v1_encode(buf, sizeof buf, INT64_C(1234567890),
                                    payload, 2, &n),
                 0);
        check("v1 size 8+2", n == 10);
        {
            static const uint8_t want[10] = {
                0x48, 0x31, 0x02, 0x00,
                0xd2, 0x02, 0x96, 0x49,
                'A', 'B'
            };
            check_bytes("v1 golden bytes", buf, want, 10);
        }
        check_rc("v1 decode", hot_wire_v1_decode(buf, 10, 0, &word, &pl, &len),
                 0);
        check("v1 decode -> tagged word era 0",
              word == hot_from_32(INT32_C(1234567890), 0));
        check("v1 payload + len", len == 2 && pl[0] == 'A' && pl[1] == 'B');
    }
    check_rc("v1 encode 2^31 (post-wrap) -> -HOT_ERANGE, never truncated",
             hot_wire_v1_encode(buf, sizeof buf, INT64_C(2147483648), NULL, 0,
                                NULL),
             HOT_ERANGE);
    {
        static const uint8_t junk[4] = { 1, 2, 3, 4 };
        check_rc("v1 encode cap too small",
                 hot_wire_v1_encode(buf, 4, INT64_C(1234567890), junk, 4,
                                    NULL),
                 HOT_ENOSPC);
    }
    {
        static const uint8_t v2[15] = {
            0x48, 0x32, 0x02, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x80,
            'A', 'B'
        };
        check_rc("v1 decoder fed v2 frame -> loud error (G4)",
                 hot_wire_v1_decode(v2, 15, 0, NULL, NULL, NULL),
                 HOT_EMALFORMED);
    }

    puts("== mixed-fleet scenario (T5 shape, in-process) ==");
    {
        uint8_t a_hello[4], b_hello[4], frame[64];
        uint8_t a_lo = 0, a_hi = 0, b_lo = 0, b_hi = 0;
        hot_time_t sent, got;
        const uint8_t *rpl = NULL;

        check("peer A hello", hot_wire_hello_encode(a_hello, sizeof a_hello,
                                                    1, 2, NULL) == 0);
        check("peer B hello", hot_wire_hello_encode(b_hello, sizeof b_hello,
                                                    1, 2, NULL) == 0);
        check("A reads B hello",
              hot_wire_hello_decode(b_hello, 4, &b_lo, &b_hi) == 0);
        check("B reads A hello",
              hot_wire_hello_decode(a_hello, 4, &a_lo, &a_hi) == 0);
        check_ver("A negotiates v2",
                  hot_wire_negotiate(1, 2, b_lo, b_hi), 2);
        check_ver("B negotiates v2",
                  hot_wire_negotiate(1, 2, a_lo, a_hi), 2);

        sent = hot_from_32(INT32_MIN, 1);
        check("A sends v2 tagged word",
              hot_wire_v2_encode(frame, sizeof frame, sent, NULL, 0, &n)
                  == 0);
        check("B decodes v2",
              hot_wire_v2_decode(frame, n, &got, &rpl, &len) == 0);
        check("v2 tagged value converts exactly", got == sent);
        check("v2 empty payload", len == 0 && rpl != NULL);
    }
    {
        uint8_t old_hello[4], frame[64];

        check("old peer (v1-only) hello",
              hot_wire_hello_encode(old_hello, sizeof old_hello, 1, 1, NULL)
                  == 0);
        check_ver("negotiate with v1-only peer",
                  hot_wire_negotiate(1, 2, 1, 1), 1);
        check_rc("A cannot v1-encode post-wrap time -> loud refusal",
                 hot_wire_v1_encode(frame, sizeof frame, INT64_C(2147483648),
                                    NULL, 0, NULL),
                 HOT_ERANGE);
        check_rc("A v1-encodes pre-wrap time",
                 hot_wire_v1_encode(frame, sizeof frame, INT64_C(1234567890),
                                    NULL, 0, NULL),
                 0);
    }

    puts("== v3 CAPS: golden bytes + loud validation (docs/08) ==");
    {
        check_rc("caps encode (level 1, be 1)",
                 hot_wire_caps_encode(buf, sizeof buf, 1, 1, &n), 0);
        check("caps size 4", n == 4);
        {
            static const uint8_t want[4] = { 0x48, 0x43, 0x01, 0x01 };
            check_bytes("caps golden bytes", buf, want, 4);
        }
        {
            uint8_t lvl = 9, be = 9;
            check_rc("caps decode", hot_wire_caps_decode(buf, 4, &lvl, &be),
                     0);
            check("caps round-trip", lvl == 1 && be == 1);
        }
        check_rc("caps encode level 3",
                 hot_wire_caps_encode(buf, sizeof buf, 3, 0, NULL),
                 HOT_EMALFORMED);
        check_rc("caps encode be 2",
                 hot_wire_caps_encode(buf, sizeof buf, 0, 2, NULL),
                 HOT_EMALFORMED);
        check_rc("caps encode cap 3",
                 hot_wire_caps_encode(buf, 3, 1, 1, NULL), HOT_ENOSPC);
        check_rc("caps decode cap 3",
                 hot_wire_caps_decode(buf, 3, NULL, NULL), HOT_EMALFORMED);
        check_rc("caps decode cap 5",
                 hot_wire_caps_decode(buf, 5, NULL, NULL), HOT_EMALFORMED);
        {
            uint8_t bad[4] = { 0x48, 0x58, 0x01, 0x01 };
            check_rc("caps decode bad magic",
                     hot_wire_caps_decode(bad, 4, NULL, NULL), HOT_EMALFORMED);
        }
        {
            uint8_t bad[4] = { 0x48, 0x43, 0x03, 0x00 };
            check_rc("caps decode level 3",
                     hot_wire_caps_decode(bad, 4, NULL, NULL), HOT_EMALFORMED);
        }
        {
            uint8_t bad[4] = { 0x48, 0x43, 0x00, 0x02 };
            check_rc("caps decode be 2",
                     hot_wire_caps_decode(bad, 4, NULL, NULL), HOT_EMALFORMED);
        }
    }

    puts("== v3 frame: docs/08 worked examples, golden bytes (T13) ==");
    {
        hot_word_ns rec = { UINT64_C(0x000000007FFFFFFF),
                            UINT32_C(0x1DCD6500) };
        check_rc("v3 encode level 1 LE (2038-01-19 03:14:07.5)",
                 hot_wire_v3_encode(buf, sizeof buf, &rec, 1, 0, NULL, 0, &n),
                 0);
        check("v3 LE size 5+12", n == 17);
        {
            static const uint8_t want[17] = {
                0x48, 0x33, 0x00, 0x00, 0x02,
                0xFF, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x65, 0xCD, 0x1D
            };
            check_bytes("v3 golden bytes (LE session, flags=02)", buf, want,
                        17);
        }
        {
            hot_word_ns got = { 0, 0 };
            check_rc("v3 decode level 1 LE",
                     hot_wire_v3_decode(buf, 17, 1, 0, &got, &pl, &len), 0);
            check("v3 LE round-trip + empty payload",
                  got.w == rec.w && got.ns == rec.ns && len == 0 && pl != NULL);
        }
        check_rc("v3 encode level 1 BE",
                 hot_wire_v3_encode(buf, sizeof buf, &rec, 1, 1, NULL, 0, &n),
                 0);
        {
            static const uint8_t want[17] = {
                0x48, 0x33, 0x00, 0x00, 0x0A,
                0x00, 0x00, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF,
                0x1D, 0xCD, 0x65, 0x00
            };
            check_bytes("v3 golden bytes (BE session, flags=0A)", buf, want,
                        17);
        }
        {
            hot_word_ns got = { 0, 0 };
            check_rc("v3 decode level 1 BE",
                     hot_wire_v3_decode(buf, 17, 1, 1, &got, &pl, &len), 0);
            check("v3 BE round-trip",
                  got.w == rec.w && got.ns == rec.ns && len == 0);
        }
    }

    puts("== v3 frame: level 2 (_as), payload round-trip, both orders ==");
    {
        hot_word_as rec = { UINT64_C(0x3FFFFFFF80000000),
                            UINT32_C(0x6CB41780),
                            UINT64_C(123456789) };
        static const uint8_t payload[3] = { 'x', 'y', 'z' };
        check_rc("v3 encode_as level 2 LE (deep 1901 + as)",
                 hot_wire_v3_encode_as(buf, sizeof buf, &rec, 2, 0, payload, 3,
                                       &n),
                 0);
        check("v3 as LE size 5+16+3", n == 24);
        {
            hot_word_as got = { 0, 0, 0 };
            check_rc("v3 decode_as level 2 LE",
                     hot_wire_v3_decode_as(buf, 24, 2, 0, &got, &pl, &len), 0);
            check("v3 as LE round-trip + payload",
                  got.w == rec.w && got.ns == rec.ns && got.as == rec.as
                      && len == 3 && pl[0] == 'x' && pl[2] == 'z');
        }
        check_rc("v3 encode_as level 2 BE",
                 hot_wire_v3_encode_as(buf, sizeof buf, &rec, 2, 1, payload, 3,
                                       &n),
                 0);
        {
            hot_word_as got = { 0, 0, 0 };
            check_rc("v3 decode_as level 2 BE",
                     hot_wire_v3_decode_as(buf, 24, 2, 1, &got, &pl, &len), 0);
            check("v3 as BE round-trip + payload",
                  got.w == rec.w && got.ns == rec.ns && got.as == rec.as
                      && len == 3 && pl[2] == 'z');
        }
        check("v3 as BE flags = 0x0E (ns+as+BE)",
              buf[4] == (HOT_WIRE_F_NS | HOT_WIRE_F_AS | HOT_WIRE_F_BE));
    }

    puts("== v3 frame: session level 0 (X12 zero fraction, X7 no deep) ==");
    {
        hot_word_ns rec = { hot_from_64(INT64_C(1234567890)), 0 };
        check_rc("v3 encode level 0",
                 hot_wire_v3_encode(buf, sizeof buf, &rec, 0, 0, NULL, 0, &n),
                 0);
        check("v3 level-0 size 5+8", n == 13);
        {
            static const uint8_t want[13] = {
                0x48, 0x33, 0x00, 0x00, 0x00,
                0xD2, 0x02, 0x96, 0x49, 0x00, 0x00, 0x00, 0x00
            };
            check_bytes("v3 golden bytes (level 0, flags=00)", buf, want, 13);
        }
        {
            hot_word_ns got = { 1, 1 };
            check_rc("v3 decode level 0",
                     hot_wire_v3_decode(buf, 13, 0, 0, &got, &pl, &len), 0);
            check("v3 level-0 round-trip: exact level-1 view, ns = 0 (X12)",
                  got.w == rec.w && got.ns == 0 && len == 0);
        }
        rec.ns = UINT32_C(0x1DCD6500);
        check_rc("v3 encode level 0 with fraction -> -HOT_ERANGE (X12)",
                 hot_wire_v3_encode(buf, sizeof buf, &rec, 0, 0, NULL, 0,
                                    NULL),
                 HOT_ERANGE);
        {
            hot_word_ns deep = { UINT64_C(0x3FFFFFFFFFFFFFFF),
                                 HOT_NS_CLASS_DEEP };
            check_rc("v3 encode level 0 deep record -> -HOT_ERANGE (X7)",
                     hot_wire_v3_encode(buf, sizeof buf, &deep, 0, 0, NULL, 0,
                                        NULL),
                     HOT_ERANGE);
        }
        {
            hot_word_as a = { hot_from_64(1), UINT32_C(500000000),
                              UINT64_C(1) };
            check_rc("v3 encode_as level 1 with as -> -HOT_ERANGE (X12)",
                     hot_wire_v3_encode_as(buf, sizeof buf, &a, 1, 0, NULL, 0,
                                           NULL),
                     HOT_ERANGE);
        }
    }

    puts("== X1: append-only levels (record prefix, exact lower view) ==");
    {
        hot_word_as hi = { UINT64_C(0x000000007FFFFFFF),
                           UINT32_C(0x1DCD6500), UINT64_C(999999999) };
        hot_word_ns lo = { UINT64_C(0x000000007FFFFFFF),
                           UINT32_C(0x1DCD6500) };
        hot_word_as h0 = { UINT64_C(0x000000007FFFFFFF),
                           UINT32_C(0x1DCD6500), UINT64_C(0) };
        static uint8_t f2[21], f1[17];

        check_rc("setup: v3 encode level 1",
                 hot_wire_v3_encode(f1, sizeof f1, &lo, 1, 0, NULL, 0, NULL),
                 0);
        check_rc("setup: v3 encode_as level 2",
                 hot_wire_v3_encode_as(f2, sizeof f2, &hi, 2, 0, NULL, 0,
                                       NULL),
                 0);
        check("flags carry the level (0x02 vs 0x06)",
              f1[4] == 0x02 && f2[4] == 0x06);
        check("level-2 record prefix = level-1 record (X1)",
              memcmp(f2 + 5, f1 + 5, 12) == 0);
        check_rc("setup: v3 encode_as level 1 (as = 0)",
                 hot_wire_v3_encode_as(f2, sizeof f2, &h0, 1, 0, NULL, 0,
                                       NULL),
                 0);
        check("as-API level 1 (as = 0) frame byte-identical to ns frame",
              memcmp(f2, f1, 17) == 0);
        {
            hot_word_as got = { 0, 0, 1 };
            check_rc("decode_as at session level 1",
                     hot_wire_v3_decode_as(f1, 17, 1, 0, &got, NULL, NULL),
                     0);
            check("level-1 frame at as API: exact view, as = 0 (X12)",
                  got.w == lo.w && got.ns == lo.ns && got.as == 0);
        }
    }

    puts("== X9: scaffolding stays LE (payload length), records flip ==");
    {
        static uint8_t pay[258];
        static uint8_t big[300];
        hot_word_ns rec = { UINT64_C(0x000000007FFFFFFF),
                            UINT32_C(0x1DCD6500) };
        pay[0] = 'p';
        pay[257] = 'q';
        check_rc("v3 LE encode with 258-byte payload",
                 hot_wire_v3_encode(big, sizeof big, &rec, 1, 0, pay, 258,
                                    &n),
                 0);
        check("v3 LE payload length is LE (02 01)",
              big[2] == 0x02 && big[3] == 0x01);
        check_rc("v3 BE encode with 258-byte payload",
                 hot_wire_v3_encode(big, sizeof big, &rec, 1, 1, pay, 258,
                                    &n),
                 0);
        check("v3 BE payload length still LE (02 01)",
              big[2] == 0x02 && big[3] == 0x01);
        check("v3 BE record bytes big-endian",
              big[5] == 0x00 && big[9] == 0x7F && big[13] == 0x1D);
        {
            hot_word_ns got = { 0, 0 };
            check_rc("v3 BE decode 275-byte frame",
                     hot_wire_v3_decode(big, 275, 1, 1, &got, &pl, &len), 0);
            check("v3 BE big-frame round-trip",
                  got.w == rec.w && got.ns == rec.ns && len == 258
                      && pl[0] == 'p' && pl[257] == 'q');
        }
    }

    puts("== v3 frame: loud rejection (X10, T14 mismatches) ==");
    {
        hot_word_ns rec = { UINT64_C(0x000000007FFFFFFF),
                            UINT32_C(0x1DCD6500) };
        hot_word_ns got = { 0, 0 };
        check_rc("setup v3 level-1 LE frame",
                 hot_wire_v3_encode(buf, sizeof buf, &rec, 1, 0, NULL, 0, &n),
                 0);
        check_rc("v3 decode at session level 0 -> mismatch",
                 hot_wire_v3_decode(buf, 17, 0, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        check_rc("v3 decode at session BE -> mismatch",
                 hot_wire_v3_decode(buf, 17, 1, 1, &got, NULL, NULL),
                 HOT_EMALFORMED);
        check_rc("v3 decode truncated",
                 hot_wire_v3_decode(buf, 16, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        check_rc("v3 decode trailing byte",
                 hot_wire_v3_decode(buf, 18, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        check_rc("v2 decoder fed H3 frame -> loud error (docs/08)",
                 hot_wire_v2_decode(buf, 17, NULL, NULL, NULL), HOT_EMALFORMED);
        check_rc("v1 decoder fed H3 frame -> loud error (docs/08)",
                 hot_wire_v1_decode(buf, 17, 0, NULL, NULL, NULL),
                 HOT_EMALFORMED);
        buf[4] = 0x12;
        check_rc("v3 decode reserved flags bit 4",
                 hot_wire_v3_decode(buf, 17, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        buf[4] = 0x02;
        buf[13] = 0x00;
        buf[14] = 0xCA;
        buf[15] = 0x9A;
        buf[16] = 0x3B;
        check_rc("v3 decode ns fraction 10^9 -> -HOT_EMALFORMED (X3)",
                 hot_wire_v3_decode(buf, 17, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        buf[14] = 0x00;
        buf[15] = 0x00;
        buf[16] = 0x80;
        check_rc("v3 decode ns class 10 -> -HOT_EMALFORMED (X4)",
                 hot_wire_v3_decode(buf, 17, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        buf[13] = 0x00;
        buf[14] = 0x65;
        buf[15] = 0xCD;
        buf[16] = 0x1D;
        buf[4] = 0x03;
        check_rc("v3 decode compat-bit mismatch",
                 hot_wire_v3_decode(buf, 17, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        buf[4] = 0x02;
        buf[0] = 'X';
        check_rc("v3 decode bad magic",
                 hot_wire_v3_decode(buf, 17, 1, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        buf[0] = 'H';
        check_rc("v3 encode cap too small",
                 hot_wire_v3_encode(buf, 16, &rec, 1, 0, NULL, 0, NULL),
                 HOT_ENOSPC);
        check_rc("v3 ns-API encode at level 2 -> -HOT_EMALFORMED (use _as)",
                 hot_wire_v3_encode(buf, sizeof buf, &rec, 2, 0, NULL, 0,
                                    NULL),
                 HOT_EMALFORMED);
        check_rc("v3 ns-API decode at level 2 -> -HOT_EMALFORMED (use _as)",
                 hot_wire_v3_decode(buf, 17, 2, 0, &got, NULL, NULL),
                 HOT_EMALFORMED);
        {
            hot_word_ns tag = { UINT64_C(0x8000000080000000),
                                UINT32_C(0x1DCD6500) };
            check_rc("setup v3 tagged frame",
                     hot_wire_v3_encode(buf, sizeof buf, &tag, 1, 0, NULL, 0,
                                        &n),
                     0);
            check("v3 tagged frame flags = 0x03 (compat carried)",
                  buf[4] == 0x03);
            buf[16] = 0x40;    /* ns raw 0x40CD6500: deep class */
            check_rc("v3 decode deep class + compat word -> -HOT_EMALFORMED (X6)",
                     hot_wire_v3_decode(buf, 17, 1, 0, &got, NULL, NULL),
                     HOT_EMALFORMED);
        }
        {
            hot_word_as a = { UINT64_C(0x000000007FFFFFFF),
                              UINT32_C(0x1DCD6500), UINT64_C(1) };
            hot_word_as gota = { 0, 0, 0 };
            check_rc("setup v3 level-2 frame",
                     hot_wire_v3_encode_as(buf, sizeof buf, &a, 2, 0, NULL, 0,
                                           &n),
                     0);
            check("setup size 21", n == 21);
            buf[4] = 0x04;
            check_rc("v3 decode as-flag without ns-flag -> -HOT_EMALFORMED",
                     hot_wire_v3_decode_as(buf, 21, 2, 0, &gota, NULL, NULL),
                     HOT_EMALFORMED);
            buf[4] = 0x06;
        }
    }

    puts("== T14: v3 negotiation matrix (HELLO -> CAPS -> session) ==");
    {
        uint8_t a_hello[4], b_hello[4], a_caps[4], b_caps[4];
        uint8_t lo = 0, hi = 0;
        uint8_t alvl = 0, abl = 0, blvl = 0, bbl = 0;

        check("A hello (1..3)",
              hot_wire_hello_encode(a_hello, sizeof a_hello, 1, 3, NULL) == 0);
        check("B hello (1..3)",
              hot_wire_hello_encode(b_hello, sizeof b_hello, 1, 3, NULL) == 0);
        check("A reads B hello",
              hot_wire_hello_decode(b_hello, 4, &lo, &hi) == 0);
        check_ver("A negotiates v3", hot_wire_negotiate(1, 3, lo, hi), 3);
        check("B reads A hello",
              hot_wire_hello_decode(a_hello, 4, &lo, &hi) == 0);
        check_ver("B negotiates v3", hot_wire_negotiate(1, 3, lo, hi), 3);

        /* A: max level 2, LE-only. B: max level 1, accepts BE. */
        check("A caps (level 2, be 0)",
              hot_wire_caps_encode(a_caps, sizeof a_caps, 2, 0, NULL) == 0);
        check("B caps (level 1, be 1)",
              hot_wire_caps_encode(b_caps, sizeof b_caps, 1, 1, NULL) == 0);
        check("A reads B caps",
              hot_wire_caps_decode(b_caps, 4, &blvl, &bbl) == 0);
        check("B reads A caps",
              hot_wire_caps_decode(a_caps, 4, &alvl, &abl) == 0);
        check("session level = min(2, 1) = 1",
              (alvl < blvl ? alvl : blvl) == 1);
        check("session order = LE (A refuses BE)", (abl && bbl) == 0);
        {
            hot_word_ns rec = { UINT64_C(0x000000007FFFFFFF),
                                UINT32_C(0x1DCD6500) };
            hot_word_ns got = { 0, 0 };
            check("A sends v3 at session level 1, LE",
                  hot_wire_v3_encode(buf, sizeof buf, &rec, 1, 0, NULL, 0, &n)
                      == 0);
            check("B decodes at session level 1, LE",
                  hot_wire_v3_decode(buf, n, 1, 0, &got, NULL, NULL) == 0);
            check("v3 session round-trip",
                  got.w == rec.w && got.ns == rec.ns);
        }

        /* A now accepts BE too: both accept -> session BE. */
        check("A caps (level 2, be 1)",
              hot_wire_caps_encode(a_caps, sizeof a_caps, 2, 1, NULL) == 0);
        check("A re-reads B caps",
              hot_wire_caps_decode(b_caps, 4, &blvl, &bbl) == 0);
        check("B re-reads A caps",
              hot_wire_caps_decode(a_caps, 4, &alvl, &abl) == 0);
        check("session level = min(2, 1) = 1",
              (alvl < blvl ? alvl : blvl) == 1);
        check("session order = BE (both accept)", (abl && bbl) == 1);
        {
            hot_word_ns rec = { UINT64_C(0x000000007FFFFFFF),
                                UINT32_C(0x1DCD6500) };
            hot_word_ns got = { 0, 0 };
            check("A sends v3 at session level 1, BE",
                  hot_wire_v3_encode(buf, sizeof buf, &rec, 1, 1, NULL, 0, &n)
                      == 0);
            check("B decodes at session level 1, BE",
                  hot_wire_v3_decode(buf, n, 1, 1, &got, NULL, NULL) == 0);
            check("v3 BE session round-trip",
                  got.w == rec.w && got.ns == rec.ns);
        }

        /* level floor: both peers at level 0. */
        check("A caps (level 0, be 0)",
              hot_wire_caps_encode(a_caps, sizeof a_caps, 0, 0, NULL) == 0);
        check("B caps (level 0, be 0)",
              hot_wire_caps_encode(b_caps, sizeof b_caps, 0, 0, NULL) == 0);
        check("A reads B caps (level 0)",
              hot_wire_caps_decode(b_caps, 4, &blvl, &bbl) == 0);
        check("B reads A caps (level 0)",
              hot_wire_caps_decode(a_caps, 4, &alvl, &abl) == 0);
        check("session level = 0", alvl == 0 && blvl == 0);
    }

    puts("== T14: v2/v1 fallback — no CAPS below v3 ==");
    {
        uint8_t old_hello[4];
        uint8_t lo = 0, hi = 0;
        check("old peer (v2-only) hello",
              hot_wire_hello_encode(old_hello, sizeof old_hello, 1, 2, NULL)
                  == 0);
        check("v3 host reads old hello",
              hot_wire_hello_decode(old_hello, 4, &lo, &hi) == 0);
        check_ver("negotiate v2 (v3 host falls back)",
                  hot_wire_negotiate(1, 3, lo, hi), 2);
        check("v2 frame on the v2 session",
              hot_wire_v2_encode(buf, sizeof buf, hot_from_64(1234567890),
                                 NULL, 0, &n) == 0);
        check("v2 decode on the v2 session",
              hot_wire_v2_decode(buf, n, &word, NULL, NULL) == 0);
        {
            uint8_t caps[4];
            check("caps encode for the sub-v3 probe",
                  hot_wire_caps_encode(caps, sizeof caps, 1, 1, NULL) == 0);
            check_rc("v2 decoder fed CAPS -> loud error (no CAPS below v3)",
                     hot_wire_v2_decode(caps, 4, NULL, NULL, NULL),
                     HOT_EMALFORMED);
        }
    }

    printf("%s: %d failures / %d checks\n",
           fails ? "TEST FAILED" : "TEST PASSED", fails, checks);
    return fails != 0;
}