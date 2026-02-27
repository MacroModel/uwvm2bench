#include "bench_common.h"

#include <math.h>
#include <stdint.h>

int main() {
    constexpr uint32_t kIters = 8000000;

    uint32_t state = 1;
    float x = 1.0f;
    float acc = 0.0f;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kIters; ++i) {
        const uint32_t r = u2bench_xorshift32(&state);
        const float a = (float)((r & 0xffffu) + 1u);

        x = x * 1.0000001f + (float)(i & 1023u) * 0.0000001f;
        acc += 1.0f / sqrtf(a + x);
        acc *= 0.9999999f;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_f64((double)acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

