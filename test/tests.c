#include <stdio.h>

#include "../utf8.h"
#include "utf8-encode.h"

static int count_pass;
static int count_fail;

#define TEST(x, s, ...) \
    do { \
        if (x) { \
            printf("\033[32;1mPASS\033[0m " s "\n", __VA_ARGS__); \
            count_pass++; \
        } else { \
            printf("\033[31;1mFAIL\033[0m " s "\n", __VA_ARGS__); \
            count_fail++; \
        } \
    } while (0)

int
main(void)
{
    /* Make sure it can decode every character */
    {
        long failures = 0;
        for (long i = 0; i < 0x10000L; i++) {
            if (!IS_SURROGATE(i)) {
                int e;
                long c;
                unsigned char buf[8] = {0};
                unsigned char *end = utf8_encode(buf, i);
                unsigned char *res = utf8_decode(buf, &c, &e);
                failures += end != res || c != i || e;
            }
        }
        TEST(failures == 0, "decode all, errors: %ld", failures);
    }

    /* Does it reject all surrogate halves? */
    {
        long failures = 0;
        for (long i = 0xD800; i <= 0xDFFFU; i++) {
            int e;
            long c;
            unsigned char buf[8] = {0};
            utf8_encode(buf, i);
            utf8_decode(buf, &c, &e);
            failures += !e;
        }
        TEST(failures == 0, "surrogate halves, errors: %ld", failures);
    }

    /* How about non-canonical encodings? */
    {
        int e;
        long c;
        unsigned char *end;

        unsigned char buf2[8] = {0xc0, 0xA4};
        end = utf8_decode(buf2, &c, &e);
        TEST(e, "non-canonical len 2, 0x%02x", e);
        TEST(end == buf2 + 2, "non-canonical recover 2, U+%04lx", c);

        unsigned char buf3[8] = {0xe0, 0x80, 0xA4};
        end = utf8_decode(buf3, &c, &e);
        TEST(e, "non-canonical len 3, 0x%02x", e);
        TEST(end == buf3 + 3, "non-canonical recover 3, U+%04lx", c);

        unsigned char buf4[8] = {0xf0, 0x80, 0x80, 0xA4};
        end = utf8_decode(buf4, &c, &e);
        TEST(e, "non-canonical encoding len 4, 0x%02x", e);
        TEST(end == buf4 + 4, "non-canonical recover 4, U+%04lx", c);
    }

    /* Let's try some bogus byte sequences */
    {
        int e;
        long c;

        unsigned char buf[4] = {0xff};
        int len = (unsigned char *)utf8_decode(buf, &c, &e) - buf;
        TEST(e, "bogus ff 00 00 00, 0x%02x, U+%04lx, len=%d", e, c, len);
    }

    printf("%d fail, %d pass\n", count_fail, count_pass);
    return count_fail != 0;
}
