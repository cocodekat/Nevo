#ifndef H1_HASH_H
#define H1_HASH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

// -------------------------
// Binary literal macro
// -------------------------
#define h1_bin(x) (strtoull(#x, NULL, 2))

// -------------------------
// Core hash function
// -------------------------
static inline unsigned long long h1_core(unsigned long long input) {
    if (input > ((~0ULL) / 255ULL)) {
        fprintf(stderr, "Error: input too big\n");
        return 0;
    }

    // Convert input to binary string
    char bits[300] = {0};
    int len = 0;
    unsigned long long tmp = input;

    if (tmp == 0) {
        bits[len++] = '0';
    } else {
        while (tmp > 0) {
            bits[len++] = (tmp & 1) ? '1' : '0';
            tmp >>= 1;
        }
        // Reverse bits to left->right
        for (int i = 0; i < len / 2; i++) {
            char c = bits[i];
            bits[i] = bits[len - 1 - i];
            bits[len - 1 - i] = c;
        }
    }
    bits[len] = '\0';

    // Step 1: Prepend 0 if first bit = 1
    if (bits[0] == '1') {
        memmove(bits + 1, bits, len + 1);
        bits[0] = '0';
        len++;
    }

    // Step 2: Pad to 8 bits on the RIGHT
    while (len < 8) bits[len++] = '0';
    bits[len] = '\0';

    // Step 3: Ripple bit swapping (overlapping sliding swaps)
    // Step 3: Ripple bit swapping (overlapping after the second swap)
    for (int i = 0; i + 1 < len; ) {
        // swap i and i+1
        char tmp = bits[i];
        bits[i] = bits[i + 1];
        bits[i + 1] = tmp;

        // after first swap (1&2), jump to 2 to start overlap from 3&4
        if (i == 0) i = 2;
        else i++; // overlap by one each time
    }




    // Step 4: Convert binary string back to integer
    unsigned long long result = 0;
    for (int i = 0; i < len; i++)
        result = (result << 1) | (bits[i] == '1');

    // Step 5: Integer square root check
    unsigned long long sq = (unsigned long long)sqrt((long double)input);
    if (sq * sq == input)
        result += sq;
    else
        result *= input;

    return result;
}

// -------------------------
// String input handler
// -------------------------
static inline unsigned long long h1_str(const char *s) {
    unsigned long long val = 0;
    for (size_t i = 0; s[i]; i++) {
        val = val * 256 + (unsigned char)s[i];
        if (val > ((~0ULL) / 255ULL)) {
            fprintf(stderr, "Error: input too big\n");
            return 0;
        }
    }
    return h1_core(val);
}

// -------------------------
// Integer input handler
// -------------------------
static inline unsigned long long h1_int(unsigned long long x) {
    return h1_core(x);
}
static inline unsigned long long h1_float(unsigned long long x) {
    return h1_core(x);
}

// -------------------------
// User-facing macro
// -------------------------
#define h1(x) _Generic((x), \
    float: h1_float, \
    int: h1_int, \
    long: h1_int, \
    long long: h1_int, \
    unsigned int: h1_int, \
    unsigned long: h1_int, \
    unsigned long long: h1_int, \
    const char*: h1_str, \
    char*: h1_str \
)(x)

// -------------------------
// Binary print helper
// -------------------------
static inline void pbin(unsigned long long n) {
    if (n == 0) {
        printf("0");
        return;
    }
    char bits[65];
    int i = 0;
    while (n > 0) {
        bits[i++] = (n & 1) ? '1' : '0';
        n >>= 1;
    }
    for (int j = i - 1; j >= 0; j--)
        putchar(bits[j]);
}

#endif
