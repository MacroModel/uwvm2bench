#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using MemcpyFn = void* (*)(void*, const void*, size_t);
static volatile MemcpyFn g_memcpy = memcpy;

int main() {
    constexpr size_t kSize = 64;
    constexpr uint32_t kIters = 5000000u;

    uint8_t* src = (uint8_t*)malloc(4096);
    uint8_t* dst = (uint8_t*)malloc(4096);
    if (!src || !dst) {
        printf("malloc failed\n");
        free(src);
        free(dst);
        return 1;
    }

    uint32_t state = 1;
    for (size_t i = 0; i < 4096; ++i) {
        src[i] = (uint8_t)u2bench_xorshift32(&state);
        dst[i] = 0;
    }

    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kIters; ++i) {
        const size_t off = (size_t)((i * 131u) & (4096u - 1u));
        const MemcpyFn fn = g_memcpy;
        fn(dst + off, src + off, kSize);
        acc += (uint64_t)dst[off] + (uint64_t)dst[(off + 31u) & (4096u - 1u)];
        src[(off + 7u) & (4096u - 1u)] ^= (uint8_t)i;
    }
    const uint64_t t1 = u2bench_now_ns();

    free(src);
    free(dst);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
