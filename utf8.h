/* Branchless UTF-8 decoder
 * Chris Wellons
 *
 * This is free and unencumbered software released into the public domain.
 */
#ifndef UTF8_H
#define UTF8_H

/* Decode the next character, C, from BUF, reporting errors in E.
 *
 * Since this is a branchless decoder, four bytes will be read from the
 * buffer regardless of the actual length of the next character. This
 * means the buffer _must_ have at least three zero-padding bytes
 * following the end of the data stream.
 *
 * Errors are reported in E, which will be non-zero if the parsed
 * character was somehow invalid: invalid byte sequence, non-canonical
 * encoding, or a surrogate half.
 */
static void *
utf8_decode(void *buf, long *c, int *e) {
    static const char utf8_lengths[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    static const unsigned char masks[] = {
        0x00, 0x7f, 0x1f, 0x0f, 0x07
    };
    static const char thresholds[] = {
        22, 0, 7, 11, 16
    };
    static const char shiftc[] = {
        0, 18, 12, 6, 0
    };
    static const char shifte[] = {
        0, 6, 4, 2, 0
    };

    unsigned char *s = buf;
    int len = utf8_lengths[s[0] >> 3];

    *c  = (s[0] & masks[len]) << 18;
    *c |= (s[1] & 0x3fU) << 12;
    *c |= (s[2] & 0x3fU) <<  6;
    *c |= (s[3] & 0x3fU) <<  0;
    *c >>= shiftc[len];

    *e  = (*c < (1L << thresholds[len]) - 1) << 6;
    *e |= ((*c >> 11) == 0x1b) << 7;
    *e |= (s[1] & 0xc0U) >> 2;
    *e |= (s[2] & 0xc0U) >> 4;
    *e |= (s[3]        ) >> 6;
    *e ^= 0x2aU;
    *e >>= shifte[len];

    return s + len;
}

#endif
