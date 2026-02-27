#include "bench_common.h"

#include <stdint.h>
#include <wasi/api.h>

int main() {
    constexpr uint32_t kOps = 200000u;

    // Warm up.
    (void)__wasi_sched_yield();

    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        const __wasi_errno_t e = __wasi_sched_yield();
        acc += (uint64_t)e;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

