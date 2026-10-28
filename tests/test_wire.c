#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

    printf("%s: %d failures / %d checks\n",
           fails ? "TEST FAILED" : "TEST PASSED", fails, checks);
    return fails != 0;
}