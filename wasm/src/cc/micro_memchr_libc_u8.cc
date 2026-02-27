#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using MemchrFn = void* (*)(const void*, int, size_t);
static volatile MemchrFn g_memchr = memchr;

int main() {
    constexpr size_t kSize = 4u * 1024u * 1024u;
    constexpr int kReps = 32;

    uint8_t* buf = (uint8_t*)malloc(kSize);
    if (!buf) {
        printf("malloc failed\n");
        return 1;
    }

    // Fill with a value that we will never search for (targets are 1..224).
    memset(buf, 0xaa, kSize);

    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        const int target = (rep * 7 + 1) & 0xff; // never 0xaa for rep < 32
        buf[kSize - 1] = (uint8_t)target;         // ensure first match at last byte
        const MemchrFn fn = g_memchr;
        void* p = fn(buf, target, kSize);
        if (p) {
            acc += (uint64_t)((uintptr_t)p - (uintptr_t)buf);
        } else {
            acc ^= 0xfeedbeefcafebabeull;
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    free(buf);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
