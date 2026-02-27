#include "bench_common.h"

#include <stdint.h>

static inline uint64_t rotl64(uint64_t x, unsigned k) {
    return (x << k) | (x >> (64u - k));
}

int main() {
    uint64_t x = 1;
    uint64_t acc = 0;
    constexpr uint32_t kIters = 20000000u;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kIters; ++i) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        x |= 1ull;

        const int pc = __builtin_popcountll((unsigned long long)x);
        const int cl = __builtin_clzll((unsigned long long)x);
        const int ct = __builtin_ctzll((unsigned long long)x);
        acc += (uint64_t)(pc + cl + ct);

        x = rotl64(x + acc + (uint64_t)i * 0x9e3779b97f4a7c15ull, 9) ^ (x >> 1);
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc ^ x);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
