#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>

static inline uint32_t u8to32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void u32to8_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// Poly1305 (26-bit limbs) for messages whose length is a multiple of 16 bytes.
static void poly1305_auth_16n(uint8_t out[16], const uint8_t* m, size_t bytes, const uint8_t key[32]) {
    const uint32_t t0 = u8to32_le(key + 0);
    const uint32_t t1 = u8to32_le(key + 4);
    const uint32_t t2 = u8to32_le(key + 8);
    const uint32_t t3 = u8to32_le(key + 12);

    uint32_t r0 = (t0) & 0x3ffffffu;
    uint32_t r1 = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03u;
    uint32_t r2 = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ffu;
    uint32_t r3 = ((t2 >> 14) | (t3 << 18)) & 0x3f03fffu;
    uint32_t r4 = (t3 >> 8) & 0x00fffffu;

    const uint32_t s1 = r1 * 5u;
    const uint32_t s2 = r2 * 5u;
    const uint32_t s3 = r3 * 5u;
    const uint32_t s4 = r4 * 5u;

    uint32_t h0 = 0, h1 = 0, h2 = 0, h3 = 0, h4 = 0;

    while (bytes >= 16) {
        const uint32_t m0 = u8to32_le(m + 0);
        const uint32_t m1 = u8to32_le(m + 4);
        const uint32_t m2 = u8to32_le(m + 8);
        const uint32_t m3 = u8to32_le(m + 12);

        h0 += (m0)&0x3ffffffu;
        h1 += ((m0 >> 26) | (m1 << 6)) & 0x3ffffffu;
        h2 += ((m1 >> 20) | (m2 << 12)) & 0x3ffffffu;
        h3 += ((m2 >> 14) | (m3 << 18)) & 0x3ffffffu;
        h4 += (m3 >> 8) & 0x3ffffffu;
        h4 += 1u << 24; // hibit

        uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
        uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
        uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
        uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
        uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

        uint32_t c = (uint32_t)(d0 >> 26);
        h0 = (uint32_t)d0 & 0x3ffffffu;
        d1 += c;
        c = (uint32_t)(d1 >> 26);
        h1 = (uint32_t)d1 & 0x3ffffffu;
        d2 += c;
        c = (uint32_t)(d2 >> 26);
        h2 = (uint32_t)d2 & 0x3ffffffu;
        d3 += c;
        c = (uint32_t)(d3 >> 26);
        h3 = (uint32_t)d3 & 0x3ffffffu;
        d4 += c;
        c = (uint32_t)(d4 >> 26);
        h4 = (uint32_t)d4 & 0x3ffffffu;
        h0 += c * 5u;
        c = h0 >> 26;
        h0 &= 0x3ffffffu;
        h1 += c;

        m += 16;
        bytes -= 16;
    }

    // Final reduction.
    uint32_t c = h1 >> 26;
    h1 &= 0x3ffffffu;
    h2 += c;
    c = h2 >> 26;
    h2 &= 0x3ffffffu;
    h3 += c;
    c = h3 >> 26;
    h3 &= 0x3ffffffu;
    h4 += c;
    c = h4 >> 26;
    h4 &= 0x3ffffffu;
    h0 += c * 5u;
    c = h0 >> 26;
    h0 &= 0x3ffffffu;
    h1 += c;

    uint32_t g0 = h0 + 5u;
    c = g0 >> 26;
    g0 &= 0x3ffffffu;
    uint32_t g1 = h1 + c;
    c = g1 >> 26;
    g1 &= 0x3ffffffu;
    uint32_t g2 = h2 + c;
    c = g2 >> 26;
    g2 &= 0x3ffffffu;
    uint32_t g3 = h3 + c;
    c = g3 >> 26;
    g3 &= 0x3ffffffu;
    uint32_t g4 = h4 + c - (1u << 26);

    const uint32_t mask = (g4 >> 31) - 1u;
    const uint32_t nmask = ~mask;
    h0 = (h0 & nmask) | (g0 & mask);
    h1 = (h1 & nmask) | (g1 & mask);
    h2 = (h2 & nmask) | (g2 & mask);
    h3 = (h3 & nmask) | (g3 & mask);
    h4 = (h4 & nmask) | (g4 & mask);

    // Serialize and add pad (key[16..31]).
    uint64_t f0 = (uint64_t)h0 | ((uint64_t)h1 << 26);
    uint64_t f1 = ((uint64_t)h1 >> 6) | ((uint64_t)h2 << 20);
    uint64_t f2 = ((uint64_t)h2 >> 12) | ((uint64_t)h3 << 14);
    uint64_t f3 = ((uint64_t)h3 >> 18) | ((uint64_t)h4 << 8);

    const uint64_t p0 = (uint64_t)u8to32_le(key + 16);
    const uint64_t p1 = (uint64_t)u8to32_le(key + 20);
    const uint64_t p2 = (uint64_t)u8to32_le(key + 24);
    const uint64_t p3 = (uint64_t)u8to32_le(key + 28);

    f0 += p0;
    f1 += p1 + (f0 >> 32);
    f0 &= 0xffffffffull;
    f2 += p2 + (f1 >> 32);
    f1 &= 0xffffffffull;
    f3 += p3 + (f2 >> 32);
    f2 &= 0xffffffffull;

    u32to8_le(out + 0, (uint32_t)f0);
    u32to8_le(out + 4, (uint32_t)f1);
    u32to8_le(out + 8, (uint32_t)f2);
    u32to8_le(out + 12, (uint32_t)f3);
}

int main() {
    constexpr size_t kMsg = 1024u * 1024u; // 1 MiB (multiple of 16)
    constexpr int kReps = 10;

    uint8_t* msg = (uint8_t*)malloc(kMsg);
    if (!msg) {
        printf("malloc failed\n");
        return 1;
    }

    uint8_t key[32];
    for (int i = 0; i < 32; ++i) {
        key[i] = (uint8_t)(i * 7 + 3);
    }

    uint32_t state = 1;
    for (size_t i = 0; i < kMsg; ++i) {
        msg[i] = (uint8_t)u2bench_xorshift32(&state);
    }

    uint8_t out[16];
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        msg[(size_t)(rep * 997) & (kMsg - 1)] ^= (uint8_t)rep;
        poly1305_auth_16n(out, msg, kMsg, key);
        acc ^= (uint64_t)u8to32_le(out + 0) | ((uint64_t)u8to32_le(out + 8) << 32);
    }
    const uint64_t t1 = u2bench_now_ns();

    free(msg);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
