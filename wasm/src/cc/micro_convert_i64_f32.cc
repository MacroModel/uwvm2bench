#include "bench_common.h"

#include <stdint.h>

int main() {
    constexpr uint32_t kIters = 12000000;

    uint64_t seed = 1;
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kIters; ++i) {
        seed = u2bench_splitmix64(seed);
        // Keep within exact f32 integer range (<= 24 bits) to avoid traps on trunc.
        int64_t x = (int64_t)(seed & 0x00ffffffu);
        if (seed & 1u) {
            x = -x;
        }

        float f = (float)x;
        f = f * 1.000001f + (float)(i & 1023u) * 0.0001f;
        const int64_t y = (int64_t)f;

        acc += (uint64_t)(uint32_t)y;
        acc ^= (uint64_t)((uint32_t)(uintptr_t)&acc) * 0x9e3779b97f4a7c15ull;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

