#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>

static inline size_t varint_put(uint8_t* dst, size_t cap, uint64_t v) {
    size_t n = 0;
    while (v >= 0x80u && n < cap) {
        dst[n++] = (uint8_t)((v & 0x7fu) | 0x80u);
        v >>= 7;
    }
    if (n < cap) {
        dst[n++] = (uint8_t)v;
    }
    return n;
}

int main() {
    constexpr size_t kBufSize = 4u * 1024u * 1024u;
    constexpr int kReps = 30;

    uint8_t* buf = (uint8_t*)malloc(kBufSize);
    if (!buf) {
        printf("malloc failed\n");
        return 1;
    }

    size_t len = 0;
    uint64_t seed = 1;
    uint32_t nvals = 0;
    while (len + 10u < kBufSize) {
        seed = u2bench_splitmix64(seed);
        uint64_t v = seed;

        // Mix small and medium values so the varint length varies.
        switch (nvals & 3u) {
        case 0:
            v &= 0x7fu; // 1 byte
            break;
        case 1:
            v &= 0x3fffu; // 2 bytes
            break;
        case 2:
            v &= 0x1fffffu; // 3 bytes
            break;
        default:
            v &= 0x0ffffffffull; // up to 6 bytes
            break;
        }

        const size_t n = varint_put(buf + len, kBufSize - len, v);
        if (n == 0) {
            break;
        }
        len += n;
        ++nvals;
    }

    uint64_t sum = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        size_t off = 0;
        while (off < len) {
            uint64_t r = 0;
            uint32_t shift = 0;
            for (;;) {
                const uint8_t b = buf[off++];
                r |= (uint64_t)(b & 0x7fu) << shift;
                if ((b & 0x80u) == 0) {
                    break;
                }
                shift += 7;
            }
            sum += r;
        }
        sum ^= (uint64_t)rep * 0x9e3779b97f4a7c15ull;
    }
    const uint64_t t1 = u2bench_now_ns();

    free(buf);
    u2bench_sink_u64(sum);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

