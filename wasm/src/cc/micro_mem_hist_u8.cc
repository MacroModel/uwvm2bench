#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>

int main() {
    constexpr size_t kSize = 4u * 1024u * 1024u;
    constexpr int kReps = 16;

    uint8_t* buf = (uint8_t*)malloc(kSize);
    if (!buf) {
        printf("malloc failed\n");
        return 1;
    }

    uint32_t state = 1;
    for (size_t i = 0; i < kSize; ++i) {
        buf[i] = (uint8_t)u2bench_xorshift32(&state);
    }

    uint32_t hist[256];
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        for (int i = 0; i < 256; ++i) {
            hist[i] = 0;
        }
        for (size_t i = 0; i < kSize; ++i) {
            hist[buf[i]]++;
        }
        for (int i = 0; i < 256; ++i) {
            acc ^= (uint64_t)hist[i] * (uint64_t)(i + 1) + (uint64_t)rep * 0x9e3779b97f4a7c15ull;
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    free(buf);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
