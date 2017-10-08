#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

#include <unistd.h> // alarm()

#include "../utf8.h"
#include "utf8-encode.h"
#include "bh-utf8.h"

#define SECONDS 6
#define BUFLEN  8 // MB

static uint32_t
pcg32(uint64_t *s)
{
    uint64_t m = 0x9b60933458e17d7d;
    uint64_t a = 0xd737232eeccdf7ed;
    *s = *s * m + a;
    int shift = 29 - (*s >> 61);
    return *s >> shift;
}

/* Generate a random codepoint whose UTF-8 length is uniformly selected. */
static long
randchar(uint64_t *s)
{
    uint32_t r = pcg32(s);
    int len = 1 + (r & 0x3);
    r >>= 2;
    switch (len) {
        case 1:
            return r % 128;
        case 2:
            return 128 + r % (2048 - 128);
        case 3:
            return 2048 + r % (65536 - 2048);
        case 4:
            return 65536 + r % (131072 - 65536);
    }
    abort();
}

static volatile sig_atomic_t running;

static void
alarm_handler(int signum)
{
    (void)signum;
    running = 0;
}

/* Fill buffer with random characters, with evenly-distributed encoded
 * lengths.
 */
static void *
buffer_fill(void *buf, size_t z)
{
    uint64_t s = 0;
    char *p = buf;
    char *end = p + z;
    while (p < end) {
        long c;
        do
            c = randchar(&s);
        while (IS_SURROGATE(c));
        p = utf8_encode(p, c);
    }
    return p;
}

static unsigned char *
utf8_simple(unsigned char *s, long *c)
{
    unsigned char *next;
    if (s[0] < 0x80) {
        *c = s[0];
        next = s + 1;
    } else if ((s[0] & 0xe0) == 0xc0) {
        *c = ((long)(s[0] & 0x1f) <<  6) |
             ((long)(s[1] & 0x3f) <<  0);
        if ((s[1] & 0xc0) != 0x80)
            *c = -1;
        next = s + 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
        *c = ((long)(s[0] & 0x0f) << 12) |
             ((long)(s[1] & 0x3f) <<  6) |
             ((long)(s[2] & 0x3f) <<  0);
        if ((s[1] & 0xc0) != 0x80 ||
            (s[2] & 0xc0) != 0x80)
            *c = -1;
        next = s + 3;
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        *c = ((long)(s[0] & 0x07) << 18) |
             ((long)(s[1] & 0x3f) << 12) |
             ((long)(s[2] & 0x3f) <<  6) |
             ((long)(s[3] & 0x3f) <<  0);
        if ((s[1] & 0xc0) != 0x80 ||
            (s[2] & 0xc0) != 0x80 ||
            (s[3] & 0xc0) != 0x80)
            *c = -1;
        next = s + 4;
    } else {
        *c = -1; // invalid
        next = s + 1; // skip this byte
    }
    if (*c >= 0xd800 && *c <= 0xdfff)
        *c = -1; // surrogate half
    return next;
}

static long
GetChar(unsigned char *Buf, size_t *Offset)
{   long Result = 0;

    if ((Buf [*Offset] & 0x80) == 0) {
        Result = Buf [*Offset];
        ++*Offset;
        return Result;
    }

    if ((Buf [*Offset] & 0xE0) == 0xC0 && (Buf [*Offset + 1] & 0xC0) == 0x80) {
        Result = ((Buf [*Offset] & 0x1F) << 6) | (Buf [*Offset + 1] & 0x3F);
        *Offset += 2;
        return Result;
    }

    if ((Buf [*Offset] & 0xF0) == 0xE0 && (Buf [*Offset + 1] & 0xC0) == 0x80 && (Buf [*Offset + 2] & 0xC0) == 0x80) {
        Result = ((Buf [*Offset] & 0x0F) << 12) | ((Buf [*Offset + 1] & 0x3F) << 6) | (Buf [*Offset + 2] & 0x3F);
        *Offset += 3;
        return Result;
    }

    if ((Buf [*Offset] & 0xF8) == 0xF0 && (Buf [*Offset + 1] & 0xC0) == 0x80 && (Buf [*Offset + 2] & 0xC0) == 0x80 && (Buf [*Offset + 3] & 0xC0) == 0x80) {
        Result = ((Buf [*Offset] & 0x07) << 18) | ((Buf [*Offset + 1] & 0x3F) << 12) | ((Buf [*Offset + 2] & 0x3F) << 6) | (Buf [*Offset + 3] & 0x3F);
        *Offset += 4;
        return Result;
    }

    // Not a correctly encoded character; return the replacement character.

    ++*Offset;
    return -1;
}

int
main(void)
{
    double rate;
    long errors, n;
    size_t z = BUFLEN * 1024L * 1024;
    unsigned char *buffer = malloc(z);
    unsigned char *end = buffer_fill(buffer, z);

    /* Benchmark johannes1971 decoder */
    running = 1;
    signal(SIGALRM, alarm_handler);
    alarm(SECONDS);
    errors = n = 0;
    do {
        long count = 0;
        size_t offset = 0;
        while (buffer + offset < end) {
            long c = GetChar(buffer, &offset);
            count++;
            if (c == -1)
                errors++;
        }
        if (buffer + offset == end) // reached the end successfully?
            n++;
    } while (running);
    rate = n * (end - buffer) / (double)SECONDS / 1024 / 1024;
    printf("johannes1971: %f MB/s, %ld errors\n", rate, errors);

    /* Benchmark the branchless decoder */
    running = 1;
    signal(SIGALRM, alarm_handler);
    alarm(SECONDS);
    errors = n = 0;
    do {
        unsigned char *p = buffer;
        int e = 0;
        uint32_t c;
        long count = 0;
        while (p < end) {
            p = utf8_decode(p, &c, &e);
            errors += !!e;  // force errors to be checked
            count++;
        }
        if (p == end) // reached the end successfully?
            n++;
    } while (running);
    rate = n * (end - buffer) / (double)SECONDS / 1024 / 1024;
    printf("branchless: %f MB/s, %ld errors\n", rate, errors);

    /* Benchmark Bjoern Hoehrmann's decoder */
    running = 1;
    signal(SIGALRM, alarm_handler);
    alarm(SECONDS);
    errors = n = 0;
    do {
        unsigned char *p = buffer;
        uint32_t c;
        uint32_t state = 0;
        long count = 0;
        for (; p < end; p++) {
            if (!bh_utf8_decode(&state, &c, *p))
                count++;
            else if (state == UTF8_REJECT)
                errors++;  // force errors to be checked
        }
        if (p == end) // reached the end successfully?
            n++;
    } while (running);
    rate = n * (end - buffer) / (double)SECONDS / 1024 / 1024;
    printf("Hoehrmann:  %f MB/s, %ld errors\n", rate, errors);

    /* Benchmark simple decoder */
    running = 1;
    signal(SIGALRM, alarm_handler);
    alarm(SECONDS);
    errors = n = 0;
    do {
        unsigned char *p = buffer;
        long c;
        long count = 0;
        while (p < end) {
            p = utf8_simple(p, &c);
            count++;
            if (c < 0)
                errors++;
        }
        if (p == end) // reached the end successfully?
            n++;
    } while (running);
    rate = n * (end - buffer) / (double)SECONDS / 1024 / 1024;
    printf("Simple:     %f MB/s, %ld errors\n", rate, errors);

    free(buffer);
}
