#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>

static inline int skip_rand_level(uint32_t* state, int max_level) {
    // Geometric distribution: P(level >= k) = 2^-(k-1)
    uint32_t x = u2bench_xorshift32(state);
    int lvl = 1;
    while ((x & 1u) == 0u && lvl < max_level) {
        ++lvl;
        x >>= 1;
    }
    return lvl;
}

int main() {
    constexpr int kMaxLevel = 16;
    constexpr uint32_t kN = 50000;
    constexpr uint32_t kOps = 400000;
    constexpr uint32_t kStride = kN + 1; // +head(0)

    uint64_t* keys = (uint64_t*)malloc((size_t)kStride * sizeof(uint64_t));
    uint32_t* next = (uint32_t*)calloc((size_t)kMaxLevel * (size_t)kStride, sizeof(uint32_t));
    if (!keys || !next) {
        printf("alloc failed\n");
        free(keys);
        free(next);
        return 1;
    }

    keys[0] = 0;
    uint64_t seed = 1;
    for (uint32_t i = 1; i <= kN; ++i) {
        seed = u2bench_splitmix64(seed);
        keys[i] = (seed ^ ((uint64_t)i << 1)) | 1ull;
    }

    auto NEXT = [&](int level, uint32_t idx) -> uint32_t& { return next[(size_t)level * (size_t)kStride + idx]; };

    uint32_t rng = 1;
    int cur_level = 1;
    uint32_t update[kMaxLevel];

    const uint64_t t0 = u2bench_now_ns();

    for (uint32_t node = 1; node <= kN; ++node) {
        const uint64_t k = keys[node];
        const int lvl = skip_rand_level(&rng, kMaxLevel);
        if (lvl > cur_level) {
            for (int l = cur_level; l < lvl; ++l) {
                update[l] = 0;
            }
            cur_level = lvl;
        }

        uint32_t x = 0;
        for (int l = cur_level - 1; l >= 0; --l) {
            uint32_t y = NEXT(l, x);
            while (y != 0 && keys[y] < k) {
                x = y;
                y = NEXT(l, x);
            }
            update[l] = x;
        }

        for (int l = 0; l < lvl; ++l) {
            NEXT(l, node) = NEXT(l, update[l]);
            NEXT(l, update[l]) = node;
        }
    }

    uint64_t sum = 0;
    for (uint32_t i = 0; i < kOps; ++i) {
        uint64_t q;
        if ((i & 1u) == 0u) {
            q = keys[1u + (uint32_t)(u2bench_splitmix64((uint64_t)i) % (uint64_t)kN)];
        } else {
            q = (u2bench_splitmix64((uint64_t)i) ^ 0xfeedbeefcafebabeull) | 1ull;
        }

        uint32_t x = 0;
        for (int l = cur_level - 1; l >= 0; --l) {
            uint32_t y = NEXT(l, x);
            while (y != 0 && keys[y] < q) {
                x = y;
                y = NEXT(l, x);
            }
        }
        const uint32_t y = NEXT(0, x);
        if (y != 0 && keys[y] == q) {
            sum += keys[y];
        } else {
            sum ^= q + 0x9e3779b97f4a7c15ull;
        }
    }

    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(sum);
    u2bench_print_time_ns(t1 - t0);

    free(keys);
    free(next);
    return 0;
}
