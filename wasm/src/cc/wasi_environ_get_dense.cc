#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <wasi/wasip1.h>

int main() {
    __wasi_size_t envc = 0;
    __wasi_size_t buf_size = 0;
    const __wasi_errno_t rc0 = __wasi_environ_sizes_get(&envc, &buf_size);
    if (rc0 != 0) {
        printf("environ_sizes_get failed: %u\n", (unsigned)rc0);
        return 1;
    }

    const size_t n_ptrs = (size_t)(envc ? envc : 1);
    const size_t n_buf = (size_t)(buf_size ? buf_size : 1);

    uint8_t** env = (uint8_t**)malloc(n_ptrs * sizeof(uint8_t*));
    uint8_t* buf = (uint8_t*)malloc(n_buf);
    if (!env || !buf) {
        printf("malloc failed\n");
        return 1;
    }

    (void)__wasi_environ_get(env, buf);

    constexpr int kIters = 200000;
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (int i = 0; i < kIters; ++i) {
        const __wasi_errno_t rc = __wasi_environ_get(env, buf);
        acc += (uint64_t)rc;
        if (buf_size) {
            acc += (uint64_t)buf[(size_t)i % n_buf];
        }
        if (envc) {
            const uint32_t p = (uint32_t)(uintptr_t)env[(size_t)i % (size_t)envc];
            acc ^= (uint64_t)p * 0x9e3779b97f4a7c15ull;
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    free(buf);
    free(env);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

