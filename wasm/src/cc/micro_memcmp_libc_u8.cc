#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using MemcmpFn = int (*)(const void*, const void*, size_t);
static volatile MemcmpFn g_memcmp = memcmp;

int main() {
    constexpr size_t kSize = 4u * 1024u * 1024u;
    constexpr int kReps = 32;

    uint8_t* a = (uint8_t*)malloc(kSize);
    uint8_t* b = (uint8_t*)malloc(kSize);
    if (!a || !b) {
        printf("malloc failed\n");
        free(a);
        free(b);
        return 1;
    }

    uint32_t state = 1;
    for (size_t i = 0; i < kSize; ++i) {
        const uint8_t v = (uint8_t)u2bench_xorshift32(&state);
        a[i] = v;
        b[i] = v;
    }

    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        const MemcmpFn fn = g_memcmp;
        const int rc = fn(a, b, kSize);
        acc ^= (uint64_t)(uint32_t)rc + (uint64_t)rep * 0x9e3779b97f4a7c15ull;
    }
    const uint64_t t1 = u2bench_now_ns();

    free(a);
    free(b);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
