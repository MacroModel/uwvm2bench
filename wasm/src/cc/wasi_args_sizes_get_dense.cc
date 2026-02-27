#include "bench_common.h"

#include <stdint.h>
#include <wasi/api.h>

int main() {
    constexpr uint32_t kOps = 200000u;
    __wasi_size_t argc = 0;
    __wasi_size_t argv_buf_size = 0;

    // Warm up + verify.
    const __wasi_errno_t e0 = __wasi_args_sizes_get(&argc, &argv_buf_size);
    if (e0 != __WASI_ERRNO_SUCCESS) {
        printf("args_sizes_get failed: e=%u\n", (unsigned)e0);
        return 1;
    }

    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        const __wasi_errno_t e = __wasi_args_sizes_get(&argc, &argv_buf_size);
        acc += (uint64_t)e + (uint64_t)argc + (uint64_t)argv_buf_size;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

