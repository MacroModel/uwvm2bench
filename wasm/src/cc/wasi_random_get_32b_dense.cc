#include "bench_common.h"

#include <stdint.h>
#include <wasi/wasip1.h>

int main() {
    constexpr int kIters = 200000;

    alignas(16) uint8_t buf[32];
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (int i = 0; i < kIters; ++i) {
        const __wasi_errno_t rc = __wasi_random_get(buf, sizeof(buf));
        acc += (uint64_t)rc;
        acc ^= (uint64_t)buf[i & 31] + (uint64_t)buf[(i * 7) & 31] * 1315423911ull;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
