#include "bench_common.h"

#include <stdint.h>

int main() {
    double a0 = 0.1, a1 = 0.2, a2 = 0.3, a3 = 0.4;
    double a4 = 0.5, a5 = 0.6, a6 = 0.7, a7 = 0.8;
    double a8 = 0.9, a9 = 1.0, a10 = 1.1, a11 = 1.2;
    double a12 = 1.3, a13 = 1.4, a14 = 1.5, a15 = 1.6;

    double acc = 0.0;
    constexpr uint32_t kIters = 5000000u;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kIters; ++i) {
        const double x = (double)((i & 1023u) + 1u) * 1.0e-6; // bounded

        a0 = a0 + (a1 - a0) * 0.000001 + x;
        a1 = a1 + (a2 - a1) * 0.000001 - x;
        a2 = a2 + (a3 - a2) * 0.000001 + x * 0.5;
        a3 = a3 + (a4 - a3) * 0.000001 - x * 0.25;

        a4 = a4 + (a5 - a4) * 0.000001 + x;
        a5 = a5 + (a6 - a5) * 0.000001 - x;
        a6 = a6 + (a7 - a6) * 0.000001 + x * 0.5;
        a7 = a7 + (a8 - a7) * 0.000001 - x * 0.25;

        a8 = a8 + (a9 - a8) * 0.000001 + x;
        a9 = a9 + (a10 - a9) * 0.000001 - x;
        a10 = a10 + (a11 - a10) * 0.000001 + x * 0.5;
        a11 = a11 + (a12 - a11) * 0.000001 - x * 0.25;

        a12 = a12 + (a13 - a12) * 0.000001 + x;
        a13 = a13 + (a14 - a13) * 0.000001 - x;
        a14 = a14 + (a15 - a14) * 0.000001 + x * 0.5;
        a15 = a15 + (a0 - a15) * 0.000001 - x * 0.25;

        acc += a0 * 0.31 + a7 * 0.17 + a15 * 0.13;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_f64(acc + a0 + a7 + a15);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
