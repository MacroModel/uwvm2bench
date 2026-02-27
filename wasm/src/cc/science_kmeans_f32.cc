#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>

static inline float u2bench_frand01(uint32_t* state) {
    const uint32_t x = u2bench_xorshift32(state);
    const uint32_t mant = x & 0x00ffffffu;
    return (float)mant * (1.0f / 16777216.0f);
}

int main() {
    constexpr int kN = 50000;
    constexpr int kK = 16;
    constexpr int kIters = 25;

    float* px = (float*)malloc((size_t)kN * sizeof(float));
    float* py = (float*)malloc((size_t)kN * sizeof(float));
    uint8_t* asg = (uint8_t*)malloc((size_t)kN * sizeof(uint8_t));
    if (!px || !py || !asg) {
        printf("malloc failed\n");
        free(px);
        free(py);
        free(asg);
        return 1;
    }

    uint32_t state = 1;
    for (int i = 0; i < kN; ++i) {
        const float x = u2bench_frand01(&state) * 2.0f - 1.0f;
        const float y = u2bench_frand01(&state) * 2.0f - 1.0f;
        px[i] = x;
        py[i] = y;
        asg[i] = (uint8_t)(i & (kK - 1));
    }

    float cx[kK];
    float cy[kK];
    for (int k = 0; k < kK; ++k) {
        cx[k] = px[k];
        cy[k] = py[k];
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int it = 0; it < kIters; ++it) {
        float sx[kK];
        float sy[kK];
        uint32_t cnt[kK];
        for (int k = 0; k < kK; ++k) {
            sx[k] = 0.0f;
            sy[k] = 0.0f;
            cnt[k] = 0;
        }

        for (int i = 0; i < kN; ++i) {
            float best = 1.0e30f;
            int bestk = 0;
            const float x = px[i];
            const float y = py[i];
            for (int k = 0; k < kK; ++k) {
                const float dx = x - cx[k];
                const float dy = y - cy[k];
                const float d = dx * dx + dy * dy;
                if (d < best) {
                    best = d;
                    bestk = k;
                }
            }
            asg[i] = (uint8_t)bestk;
            sx[bestk] += x;
            sy[bestk] += y;
            cnt[bestk]++;
        }

        for (int k = 0; k < kK; ++k) {
            if (cnt[k]) {
                const float inv = 1.0f / (float)cnt[k];
                cx[k] = sx[k] * inv;
                cy[k] = sy[k] * inv;
            } else {
                // Re-seed empty cluster.
                const int idx = (int)(u2bench_xorshift32(&state) % (uint32_t)kN);
                cx[k] = px[idx];
                cy[k] = py[idx];
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int k = 0; k < kK; ++k) {
        acc += (double)cx[k] * 0.7 + (double)cy[k] * 1.3 + (double)k;
    }
    for (int i = 0; i < kN; i += 257) {
        acc += (double)asg[i];
    }

    free(px);
    free(py);
    free(asg);

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
