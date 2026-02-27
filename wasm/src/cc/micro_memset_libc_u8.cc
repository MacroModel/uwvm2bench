#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using MemsetFn = void* (*)(void*, int, size_t);
static volatile MemsetFn g_memset = memset;

int main() {
    constexpr size_t kSize = 4u * 1024u * 1024u;
    constexpr int kReps = 32;

    uint8_t* buf = (uint8_t*)malloc(kSize);
    if (!buf) {
        printf("malloc failed\n");
        return 1;
    }

    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        const int v = (rep * 17 + 3) & 0xff;
        const MemsetFn fn = g_memset;
        fn(buf, v, kSize);
        acc += buf[(size_t)(rep * 997) & (kSize - 1)];
        acc ^= (uint64_t)(uint32_t)v * 0x9e3779b97f4a7c15ull;
    }
    const uint64_t t1 = u2bench_now_ns();

    free(buf);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
