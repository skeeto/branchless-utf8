#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

#include <unistd.h> // alarm()

#include "../utf8.h"
#include "utf8-encode.h"
#include "bj-utf8.h"

#define SECONDS 5
#define BUFLEN  64 // MB

static uint32_t
pcg32(uint64_t *s)
{
    uint64_t m = 0x9b60933458e17d7d;
    uint64_t a = 0xd737232eeccdf7ed;
    *s = *s * m + a;
    int shift = 29 - (*s >> 61);
    return *s >> shift;
}

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

int
main(void)
{
    long errors, n;
    size_t z = BUFLEN * 1024 * 1024;
    unsigned char *buffer = malloc(z);
    unsigned char *end = buffer_fill(buffer, z);

    /* Benchmark the branchless decoder */
    running = 1;
    signal(SIGALRM, alarm_handler);
    alarm(SECONDS);
    errors = n = 0;
    do {
        unsigned char *p = buffer;
        int e = 0;
        long c;
        long count = 0;
        while (p < end) {
            p = utf8_decode(p, &c, &e);
            count++;
        }
        errors += !!e;
        if (p == end)
            n++;
    } while (running);

    double rate = n * (end - buffer) / (double)SECONDS / 1024 / 1024;
    printf("branchless: %f MB/s, %ld rounds, %ld errors\n", rate, n, errors);

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
        for (; p < end; p++)
            if (!bj_utf8_decode(&state, &c, *p))
                count++;
        errors += state != UTF8_ACCEPT;
        if (p == end)
            n++;
    } while (running);

    rate = n * (end - buffer) / (double)SECONDS / 1024 / 1024;
    printf("Hoehrmann:  %f MB/s, %ld rounds, %ld errors\n", rate, n, errors);

    free(buffer);
}
