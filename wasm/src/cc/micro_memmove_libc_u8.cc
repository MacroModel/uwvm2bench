#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

using MemmoveFn = void* (*)(void*, const void*, size_t);
static volatile MemmoveFn g_memmove = memmove;

int main() {
    constexpr size_t kN = 4u * 1024u * 1024u;
    constexpr int kReps = 24; // total moved ~= 2*(kN-64)*kReps ~= 192 MiB

    uint8_t* buf = (uint8_t*)malloc(kN);
    if (!buf) {
        printf("malloc failed\n");
        return 1;
    }

    uint32_t state = 1;
    for (size_t i = 0; i < kN; ++i) {
        buf[i] = (uint8_t)u2bench_xorshift32(&state);
    }

    constexpr size_t kOff = 64;
    uint64_t acc = 0;
    const uint64_t t0 = u2bench_now_ns();
    for (int rep = 0; rep < kReps; ++rep) {
        // Overlapping copies both directions to force true memmove semantics.
        const MemmoveFn fn = g_memmove;
        fn(buf + kOff, buf, kN - kOff);
        fn(buf, buf + kOff, kN - kOff);

        const size_t i0 = ((size_t)rep * 2654435761u) & (kN - 1);
        const size_t i1 = ((size_t)rep * 11400714819323198485ull + 97u) & (kN - 1);
        buf[i0] ^= (uint8_t)(rep * 13);
        buf[i1] += (uint8_t)(rep * 7);
        acc += (uint64_t)buf[i0] + (uint64_t)buf[i1];
    }
    const uint64_t t1 = u2bench_now_ns();

    for (size_t i = 0; i < kN; i += 64) {
        acc += buf[i];
    }

    free(buf);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

