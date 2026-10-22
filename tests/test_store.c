#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

    printf("%s: %d failures / %d checks\n",
           fails ? "TEST FAILED" : "TEST PASSED", fails, checks);
    return fails != 0;
}