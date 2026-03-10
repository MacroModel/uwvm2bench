#include "bench_common.h"

#include <math.h>
#include <stdint.h>

#ifndef U2BENCH_EXTRA_KIND
#define U2BENCH_EXTRA_KIND 1
#endif

static inline double u2bench_rand_sym(uint64_t* s) {
    *s = u2bench_splitmix64(*s);
    const uint64_t x = *s;
    const double u = (double)(x & 0xffffffffu) / 4294967296.0;
    return u * 2.0 - 1.0;
}

#if U2BENCH_EXTRA_KIND == 1

int main() {
    constexpr int kW = 40;
    constexpr int kH = 40;
    constexpr int kN = kW * kH;
    constexpr int kSteps = 36;
    constexpr int kRelaxIters = 12;
    constexpr double kCap = 0.88;
    constexpr double kLeak = 0.03;
    constexpr double kBaseG = 0.16;
    constexpr double kColdG = 0.01;
    constexpr double kHotG = 0.10;
    constexpr double kThreshold = 0.18;
    constexpr double kOmega = 0.82;

    static double state[kN];
    static double work[kN];
    static double bias[kN];

    uint64_t seed = 1;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            const double px = (double)x / (double)(kW - 1) * 2.0 - 1.0;
            const double py = (double)y / (double)(kH - 1) * 2.0 - 1.0;
            const double rnd = u2bench_rand_sym(&seed);
            bias[idx] = 0.35 * px - 0.22 * py + rnd * 0.05;
            state[idx] = bias[idx] * 0.12;
            work[idx] = state[idx];
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kN; ++i) {
            work[i] = state[i];
        }

        const double pulse = ((step & 1) == 0) ? 0.03 : -0.02;
        for (int x = 0; x < kW; ++x) {
            work[x] = 0.62 * bias[x] + pulse;
            work[(kH - 1) * kW + x] = -0.38 * bias[(kH - 1) * kW + x] - pulse * 0.5;
        }
        for (int y = 0; y < kH; ++y) {
            work[y * kW] = 0.55 * bias[y * kW] - pulse * 0.5;
            work[y * kW + (kW - 1)] = -0.42 * bias[y * kW + (kW - 1)] + pulse;
        }

        for (int iter = 0; iter < kRelaxIters; ++iter) {
            for (int y = 1; y < kH - 1; ++y) {
                for (int x = 1; x < kW - 1; ++x) {
                    const int idx = y * kW + x;
                    const double v = work[idx];
                    const double north = work[idx - kW];
                    const double south = work[idx + kW];
                    const double west = work[idx - 1];
                    const double east = work[idx + 1];

                    double rhs = kCap * state[idx] + kLeak * bias[idx];
                    double gsum = kCap + kLeak;

                    double g = kBaseG + ((fabs(north - v) > kThreshold) ? kHotG : kColdG);
                    rhs += g * north;
                    gsum += g;

                    g = kBaseG + ((fabs(south - v) > kThreshold) ? kHotG : kColdG);
                    rhs += g * south;
                    gsum += g;

                    g = kBaseG + ((fabs(west - v) > kThreshold) ? kHotG : kColdG);
                    rhs += g * west;
                    gsum += g;

                    g = kBaseG + ((fabs(east - v) > kThreshold) ? kHotG : kColdG);
                    rhs += g * east;
                    gsum += g;

                    double source = 0.0;
                    if (((x + step) % 11) == 0 && (y % 5) == 2) {
                        source += 0.04;
                    }
                    if (((y + step * 2) % 13) == 0 && (x % 7) == 3) {
                        source -= 0.03;
                    }

                    const double target = (rhs + source) / gsum;
                    work[idx] = v + kOmega * (target - v);
                }
            }
        }

        for (int i = 0; i < kN; ++i) {
            state[i] = work[i];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += state[i] * state[i] + 0.1 * state[i] * bias[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 2

static inline void poisson_apply(const double* src, double* dst) {
    constexpr int kW = 64;
    constexpr int kH = 64;

    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            if (x == 0 || y == 0 || x == kW - 1 || y == kH - 1) {
                dst[idx] = src[idx];
                continue;
            }
            dst[idx] = 4.0 * src[idx] - src[idx - 1] - src[idx + 1] - src[idx - kW] - src[idx + kW];
        }
    }
}

static inline double dot_product(const double* a, const double* b, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

int main() {
    constexpr int kW = 64;
    constexpr int kH = 64;
    constexpr int kN = kW * kH;
    constexpr int kSolves = 3;
    constexpr int kIters = 28;

    static double x[kN];
    static double r[kN];
    static double p[kN];
    static double ap[kN];
    static double b[kN];

    uint64_t seed = 3;
    for (int y = 0; y < kH; ++y) {
        for (int x0 = 0; x0 < kW; ++x0) {
            const int idx = y * kW + x0;
            if (x0 == 0 || y == 0 || x0 == kW - 1 || y == kH - 1) {
                b[idx] = 0.0;
                continue;
            }
            double src = u2bench_rand_sym(&seed) * 0.05;
            if ((x0 > 12 && x0 < 22) && (y > 14 && y < 26)) {
                src += 0.75;
            }
            if ((x0 > 37 && x0 < 49) && (y > 32 && y < 44)) {
                src -= 0.68;
            }
            b[idx] = src;
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int solve = 0; solve < kSolves; ++solve) {
        const double scale = 1.0 + 0.12 * (double)solve;
        for (int i = 0; i < kN; ++i) {
            x[i] = 0.0;
            r[i] = b[i] * scale;
            p[i] = r[i];
        }

        double rr = dot_product(r, r, kN);
        for (int iter = 0; iter < kIters; ++iter) {
            poisson_apply(p, ap);
            const double pap = dot_product(p, ap, kN) + 1e-18;
            const double alpha = rr / pap;

            for (int i = 0; i < kN; ++i) {
                x[i] += alpha * p[i];
                r[i] -= alpha * ap[i];
            }

            const double rr_new = dot_product(r, r, kN);
            const double beta = rr_new / (rr + 1e-30);
            for (int i = 0; i < kN; ++i) {
                p[i] = r[i] + beta * p[i];
            }
            rr = rr_new;
        }

        for (int i = 0; i < kN; ++i) {
            b[i] = 0.94 * b[i] + 0.06 * x[i];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += x[i] * x[i] + 0.01 * b[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 3

int main() {
    constexpr int kW = 96;
    constexpr int kH = 96;
    constexpr int kN = kW * kH;
    constexpr int kSteps = 60;
    constexpr float kDu = 0.16f;
    constexpr float kDv = 0.08f;

    static float ua[kN];
    static float va[kN];
    static float ub[kN];
    static float vb[kN];

    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            ua[idx] = 1.0f;
            va[idx] = 0.0f;
            ub[idx] = 1.0f;
            vb[idx] = 0.0f;
        }
    }

    for (int y = 38; y < 58; ++y) {
        for (int x = 38; x < 58; ++x) {
            const int idx = y * kW + x;
            ua[idx] = 0.28f;
            va[idx] = 0.82f;
        }
    }

    float* u = ua;
    float* v = va;
    float* un = ub;
    float* vn = vb;

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        const float feed = 0.024f + 0.0006f * (float)(step & 7);
        const float kill = 0.053f + 0.0005f * (float)(step % 5);

        for (int x = 0; x < kW; ++x) {
            un[x] = u[x];
            vn[x] = v[x];
            un[(kH - 1) * kW + x] = u[(kH - 1) * kW + x];
            vn[(kH - 1) * kW + x] = v[(kH - 1) * kW + x];
        }
        for (int y = 0; y < kH; ++y) {
            un[y * kW] = u[y * kW];
            vn[y * kW] = v[y * kW];
            un[y * kW + (kW - 1)] = u[y * kW + (kW - 1)];
            vn[y * kW + (kW - 1)] = v[y * kW + (kW - 1)];
        }

        for (int y = 1; y < kH - 1; ++y) {
            for (int x = 1; x < kW - 1; ++x) {
                const int idx = y * kW + x;
                const float cu = u[idx];
                const float cv = v[idx];
                const float lap_u = u[idx - 1] + u[idx + 1] + u[idx - kW] + u[idx + kW] - 4.0f * cu;
                const float lap_v = v[idx - 1] + v[idx + 1] + v[idx - kW] + v[idx + kW] - 4.0f * cv;
                const float uvv = cu * cv * cv;

                float nu = cu + kDu * lap_u - uvv + feed * (1.0f - cu);
                float nv = cv + kDv * lap_v + uvv - (feed + kill) * cv;
                if (nu < 0.0f) nu = 0.0f;
                if (nv < 0.0f) nv = 0.0f;
                if (nu > 1.25f) nu = 1.25f;
                if (nv > 1.25f) nv = 1.25f;

                un[idx] = nu;
                vn[idx] = nv;
            }
        }

        float* tmp = u;
        u = un;
        un = tmp;
        tmp = v;
        v = vn;
        vn = tmp;
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += (double)u[i] * 0.75 + (double)v[i] * 1.25;
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 4

int main() {
    constexpr int kN = 4096;
    constexpr int kDeg = 12;
    constexpr int kEdges = kN * kDeg;
    constexpr int kSteps = 36;
    constexpr double kDamp = 0.85;
    constexpr double kBase = (1.0 - kDamp) / (double)kN;

    static uint32_t dst[kEdges];
    static double rank[kN];
    static double next_rank[kN];

    uint64_t seed = 7;
    for (int i = 0; i < kN; ++i) {
        rank[i] = 1.0 / (double)kN;
        for (int e = 0; e < kDeg; ++e) {
            seed = u2bench_splitmix64(seed + (uint64_t)i * 1315423911ull + (uint64_t)e);
            uint32_t v = (uint32_t)seed & (kN - 1);
            if (v == (uint32_t)i) {
                v = (v + (uint32_t)e + 1u) & (kN - 1);
            }
            dst[i * kDeg + e] = v;
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kN; ++i) {
            next_rank[i] = kBase;
        }

        for (int src = 0; src < kN; ++src) {
            const double contrib = (kDamp * rank[src]) / (double)kDeg;
            const int off = src * kDeg;
            for (int e = 0; e < kDeg; ++e) {
                next_rank[dst[off + e]] += contrib;
            }
        }

        const double mix = 0.02 + 0.001 * (double)(step & 3);
        for (int i = 0; i < kN; ++i) {
            rank[i] = (1.0 - mix) * next_rank[i] + mix * rank[(i * 17 + step * 13) & (kN - 1)];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += rank[i] * (double)((i & 63) + 1);
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 5

int main() {
    constexpr int kState = 6;
    constexpr int kObs = 2;
    constexpr int kSteps = 40000;
    constexpr double kDt = 0.05;
    constexpr double kHalfDt2 = 0.5 * kDt * kDt;

    double x[kState] = {0.0, 0.0, 0.2, -0.15, 0.01, -0.008};
    double xp[kState];
    double p[kState * kState] = {0.0};
    double pp[kState * kState] = {0.0};
    double tmp[kState * kState] = {0.0};

    for (int i = 0; i < kState; ++i) {
        p[i * kState + i] = 1.0 + 0.1 * (double)i;
    }

    const double f[kState * kState] = {
        1.0, 0.0, kDt, 0.0, kHalfDt2, 0.0,
        0.0, 1.0, 0.0, kDt, 0.0, kHalfDt2,
        0.0, 0.0, 1.0, 0.0, kDt, 0.0,
        0.0, 0.0, 0.0, 1.0, 0.0, kDt,
        0.0, 0.0, 0.0, 0.0, 0.985, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0, 0.985,
    };
    const double qdiag[kState] = {1e-4, 1e-4, 8e-4, 8e-4, 2e-3, 2e-3};
    const double r00 = 0.06;
    const double r11 = 0.06;

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        xp[0] = x[0] + kDt * x[2] + kHalfDt2 * x[4];
        xp[1] = x[1] + kDt * x[3] + kHalfDt2 * x[5];
        xp[2] = x[2] + kDt * x[4];
        xp[3] = x[3] + kDt * x[5];
        xp[4] = 0.985 * x[4] + 0.015 * sin(0.013 * (double)step);
        xp[5] = 0.985 * x[5] + 0.015 * cos(0.011 * (double)step);

        for (int i = 0; i < kState; ++i) {
            for (int j = 0; j < kState; ++j) {
                double s = 0.0;
                for (int k = 0; k < kState; ++k) {
                    s += f[i * kState + k] * p[k * kState + j];
                }
                tmp[i * kState + j] = s;
            }
        }
        for (int i = 0; i < kState; ++i) {
            for (int j = 0; j < kState; ++j) {
                double s = 0.0;
                for (int k = 0; k < kState; ++k) {
                    s += tmp[i * kState + k] * f[j * kState + k];
                }
                pp[i * kState + j] = s;
            }
            pp[i * kState + i] += qdiag[i];
        }

        const double z0 = 9.0 * sin(0.010 * (double)step) + 0.45 * cos(0.071 * (double)step);
        const double z1 = 7.5 * cos(0.012 * (double)step) + 0.35 * sin(0.063 * (double)step);
        const double y0 = z0 - xp[0];
        const double y1 = z1 - xp[1];

        const double s00 = pp[0] + r00;
        const double s01 = pp[1];
        const double s10 = pp[kState];
        const double s11 = pp[kState + 1] + r11;
        const double det = s00 * s11 - s01 * s10 + 1e-18;
        const double inv00 = s11 / det;
        const double inv01 = -s01 / det;
        const double inv10 = -s10 / det;
        const double inv11 = s00 / det;

        double k_gain[kState * kObs];
        for (int i = 0; i < kState; ++i) {
            const double p0 = pp[i * kState + 0];
            const double p1 = pp[i * kState + 1];
            k_gain[i * kObs + 0] = p0 * inv00 + p1 * inv10;
            k_gain[i * kObs + 1] = p0 * inv01 + p1 * inv11;
        }

        for (int i = 0; i < kState; ++i) {
            x[i] = xp[i] + k_gain[i * kObs + 0] * y0 + k_gain[i * kObs + 1] * y1;
        }

        for (int i = 0; i < kState; ++i) {
            for (int j = 0; j < kState; ++j) {
                p[i * kState + j] =
                    pp[i * kState + j]
                    - k_gain[i * kObs + 0] * pp[0 * kState + j]
                    - k_gain[i * kObs + 1] * pp[1 * kState + j];
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kState; ++i) {
        acc += x[i] * (double)(i + 1);
    }
    for (int i = 0; i < kState; ++i) {
        acc += p[i * kState + i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 6

int main() {
    constexpr int kRows = 4096;
    constexpr int kPerRow = 9;
    constexpr int kNnz = kRows * kPerRow;
    constexpr int kIters = 72;

    static uint32_t col_idx[kNnz];
    static double values[kNnz];
    static double x[kRows];
    static double y[kRows];

    uint64_t seed = 11;
    for (int r = 0; r < kRows; ++r) {
        x[r] = 0.25 + 0.001 * (double)(r & 31);
        const int off = r * kPerRow;
        col_idx[off] = (uint32_t)r;
        values[off] = 1.8;
        for (int e = 1; e < kPerRow; ++e) {
            seed = u2bench_splitmix64(seed + (uint64_t)r * 11400714819323198485ull + (uint64_t)e);
            col_idx[off + e] = (uint32_t)seed & (kRows - 1);
            values[off + e] = 0.02 + 0.11 * ((double)((seed >> 8) & 1023u) / 1023.0);
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int iter = 0; iter < kIters; ++iter) {
        for (int r = 0; r < kRows; ++r) {
            const int off = r * kPerRow;
            double acc = 0.0;
            for (int e = 0; e < kPerRow; ++e) {
                acc += values[off + e] * x[col_idx[off + e]];
            }
            y[r] = acc;
        }

        const double mix = 0.035 + 0.003 * (double)(iter & 3);
        double norm = 0.0;
        for (int r = 0; r < kRows; ++r) {
            const double v = (1.0 - mix) * y[r] + mix * x[(r * 29 + iter * 7) & (kRows - 1)];
            x[r] = v;
            norm += v * v;
        }
        const double inv_norm = 1.0 / sqrt(norm + 1e-18);
        for (int r = 0; r < kRows; ++r) {
            x[r] *= inv_norm;
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int r = 0; r < kRows; ++r) {
        acc += x[r] * (double)((r & 15) + 1);
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 7

int main() {
    constexpr int kW = 96;
    constexpr int kH = 96;
    constexpr int kN = kW * kH;
    constexpr int kIters = 80;
    constexpr double kGamma = 0.985;

    static double value[kN];
    static double next_value[kN];
    static double reward[kN];

    uint64_t seed = 13;
    for (int y = 0; y < kH; ++y) {
        for (int x0 = 0; x0 < kW; ++x0) {
            const int idx = y * kW + x0;
            seed = u2bench_splitmix64(seed + (uint64_t)idx * 0x9e3779b97f4a7c15ull);
            reward[idx] = -0.02 + 0.08 * ((double)(seed & 1023u) / 1023.0);
            if ((x0 > 72 && y > 72) || (x0 > 12 && x0 < 22 && y > 64 && y < 84)) {
                reward[idx] += 1.5;
            }
            if ((x0 > 38 && x0 < 60 && y > 20 && y < 40) || ((x0 + y) % 29 == 0)) {
                reward[idx] -= 1.0;
            }
            value[idx] = reward[idx] * 0.2;
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int iter = 0; iter < kIters; ++iter) {
        for (int y = 0; y < kH; ++y) {
            for (int x0 = 0; x0 < kW; ++x0) {
                const int idx = y * kW + x0;
                double best = value[idx];
                if (x0 > 0) {
                    const double cand = -0.01 + value[idx - 1];
                    if (cand > best) best = cand;
                }
                if (x0 + 1 < kW) {
                    const double cand = -0.01 + value[idx + 1];
                    if (cand > best) best = cand;
                }
                if (y > 0) {
                    const double cand = -0.01 + value[idx - kW];
                    if (cand > best) best = cand;
                }
                if (y + 1 < kH) {
                    const double cand = -0.01 + value[idx + kW];
                    if (cand > best) best = cand;
                }
                const double stay = -0.004 + value[idx];
                if (stay > best) best = stay;
                next_value[idx] = reward[idx] + kGamma * best;
            }
        }
        for (int i = 0; i < kN; ++i) {
            value[i] = next_value[i];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += value[i] * (double)((i & 7) + 1);
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 8

int main() {
    constexpr int kN = 4096;
    constexpr int kDeg = 8;
    constexpr int kEdges = kN * kDeg;
    constexpr int kRounds = 48;
    constexpr double kInf = 1.0e100;

    static uint32_t dst[kEdges];
    static double weight[kEdges];
    static double dist[kN];
    static double next_dist[kN];

    uint64_t seed = 17;
    for (int u = 0; u < kN; ++u) {
        const int off = u * kDeg;
        dst[off] = (uint32_t)((u + 1) & (kN - 1));
        weight[off] = 0.18 + 0.01 * (double)(u & 7);
        for (int e = 1; e < kDeg; ++e) {
            seed = u2bench_splitmix64(seed + (uint64_t)u * 0x9e3779b97f4a7c15ull + (uint64_t)e * 17ull);
            dst[off + e] = (uint32_t)seed & (kN - 1);
            weight[off + e] = 0.05 + 0.20 * ((double)((seed >> 11) & 1023u) / 1023.0);
        }
        dist[u] = kInf;
    }
    dist[0] = 0.0;

    const uint64_t t0 = u2bench_now_ns();
    for (int round = 0; round < kRounds; ++round) {
        for (int i = 0; i < kN; ++i) {
            next_dist[i] = dist[i];
        }

        for (int u = 0; u < kN; ++u) {
            const double du = dist[u];
            if (!(du < kInf * 0.5)) {
                continue;
            }
            const int off = u * kDeg;
            for (int e = 0; e < kDeg; ++e) {
                const uint32_t v = dst[off + e];
                const double cand = du + weight[off + e];
                if (cand < next_dist[v]) {
                    next_dist[v] = cand;
                }
            }
        }

        const double blend = 0.0005 * (double)((round & 3) + 1);
        for (int i = 0; i < kN; ++i) {
            dist[i] = next_dist[i] + blend * (double)(i & 1);
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += dist[i] / (double)((i & 15) + 1);
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 9

int main() {
    constexpr int kW = 128;
    constexpr int kH = 128;
    constexpr int kN = kW * kH;
    constexpr int kSweeps = 28;
    constexpr double kBeta = 0.34;

    static int8_t spin[kN];
    uint32_t rng = 1u;

    for (int i = 0; i < kN; ++i) {
        rng = u2bench_xorshift32(&rng);
        spin[i] = (rng & 1u) ? (int8_t)1 : (int8_t)-1;
    }

    const uint32_t thr4 = (uint32_t)(exp(-4.0 * kBeta) * 4294967295.0);
    const uint32_t thr8 = (uint32_t)(exp(-8.0 * kBeta) * 4294967295.0);

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        for (int parity = 0; parity < 2; ++parity) {
            for (int y = 1; y < kH - 1; ++y) {
                int x = 1 + ((y + parity) & 1);
                for (; x < kW - 1; x += 2) {
                    const int idx = y * kW + x;
                    const int s = (int)spin[idx];
                    const int nb =
                        (int)spin[idx - 1] +
                        (int)spin[idx + 1] +
                        (int)spin[idx - kW] +
                        (int)spin[idx + kW];
                    const int field = (((x * 13 + y * 7 + sweep * 5) & 31) == 0) ? 1 : 0;
                    const int delta = 2 * s * (nb + field);

                    bool accept = false;
                    if (delta <= 0) {
                        accept = true;
                    } else {
                        rng = u2bench_xorshift32(&rng);
                        if (delta == 4) {
                            accept = rng < thr4;
                        } else if (delta >= 8) {
                            accept = rng < thr8;
                        }
                    }

                    if (accept) {
                        spin[idx] = (int8_t)-s;
                    }
                }
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    int64_t mag = 0;
    for (int i = 0; i < kN; ++i) {
        mag += (int64_t)spin[i];
    }

    u2bench_sink_u64((uint64_t)(mag ^ 0x9e3779b97f4a7c15ull));
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 10

int main() {
    constexpr int kPaths = 4096;
    constexpr int kSteps = 96;
    constexpr double kDt = 1.0 / 96.0;
    constexpr double kSqrtDt = 0.10206207261596575;
    constexpr double kR = 0.02;
    constexpr double kKappa = 2.1;
    constexpr double kTheta = 0.045;
    constexpr double kSigma = 0.55;
    constexpr double kRho = -0.6;
    constexpr double kRhoOrth = 0.8;

    static double spot[kPaths];
    static double vol[kPaths];

    uint64_t seed = 19;
    for (int i = 0; i < kPaths; ++i) {
        spot[i] = 78.0 + 0.6 * (double)(i & 63);
        vol[i] = 0.028 + 0.0008 * (double)(i & 31);
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kPaths; ++i) {
            const double z1 = u2bench_rand_sym(&seed) + 0.5 * u2bench_rand_sym(&seed);
            const double z2 = u2bench_rand_sym(&seed) + 0.5 * u2bench_rand_sym(&seed);
            const double w1 = z1;
            const double w2 = kRho * z1 + kRhoOrth * z2;

            double vi = vol[i];
            if (vi < 1e-9) {
                vi = 1e-9;
            }
            const double sqrt_v = sqrt(vi);
            const double dv = kKappa * (kTheta - vi) * kDt + kSigma * sqrt_v * kSqrtDt * w2;
            vi += dv;
            if (vi < 1e-9) {
                vi = 1e-9;
            }
            vol[i] = vi;

            const double drift = (kR - 0.5 * vi) * kDt;
            const double diff = sqrt(vi) * kSqrtDt * w1;
            spot[i] *= exp(drift + diff);
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double payoff = 0.0;
    for (int i = 0; i < kPaths; ++i) {
        const double s = spot[i];
        const double call = fmax(s - 100.0, 0.0);
        const double put = fmax(92.0 - s, 0.0);
        payoff += call + 0.35 * put;
    }
    payoff *= exp(-kR * (double)kSteps * kDt) / (double)kPaths;

    u2bench_sink_f64(payoff);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 11

int main() {
    constexpr int kN = 320;
    constexpr int kSteps = 48;
    constexpr double kPerception2 = 0.035 * 0.035;
    constexpr double kSeparation2 = 0.008 * 0.008;
    constexpr double kMaxSpeed = 0.010;

    static float x[kN];
    static float y[kN];
    static float vx[kN];
    static float vy[kN];
    static float nx[kN];
    static float ny[kN];
    static float nvx[kN];
    static float nvy[kN];

    uint64_t seed = 23;
    for (int i = 0; i < kN; ++i) {
        x[i] = (float)(0.1 + 0.8 * ((double)(i & 31) / 31.0));
        y[i] = (float)(0.1 + 0.8 * ((double)((i * 7) & 31) / 31.0));
        vx[i] = (float)(0.002 * u2bench_rand_sym(&seed));
        vy[i] = (float)(0.002 * u2bench_rand_sym(&seed));
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kN; ++i) {
            float align_x = 0.0f;
            float align_y = 0.0f;
            float coh_x = 0.0f;
            float coh_y = 0.0f;
            float sep_x = 0.0f;
            float sep_y = 0.0f;
            int count = 0;

            for (int j = 0; j < kN; ++j) {
                if (j == i) {
                    continue;
                }
                const float dx = x[j] - x[i];
                const float dy = y[j] - y[i];
                const float d2 = dx * dx + dy * dy;
                if (d2 < kPerception2) {
                    align_x += vx[j];
                    align_y += vy[j];
                    coh_x += x[j];
                    coh_y += y[j];
                    ++count;
                    if (d2 < kSeparation2) {
                        sep_x -= dx;
                        sep_y -= dy;
                    }
                }
            }

            float ax = 0.0f;
            float ay = 0.0f;
            if (count > 0) {
                const float inv = 1.0f / (float)count;
                align_x *= inv;
                align_y *= inv;
                coh_x = coh_x * inv - x[i];
                coh_y = coh_y * inv - y[i];
                ax += 0.045f * (align_x - vx[i]);
                ay += 0.045f * (align_y - vy[i]);
                ax += 0.018f * coh_x + 0.085f * sep_x;
                ay += 0.018f * coh_y + 0.085f * sep_y;
            }

            const float swirl = ((i + step) & 1) ? 0.0006f : -0.0006f;
            float vxn = 0.985f * vx[i] + ax - swirl * y[i];
            float vyn = 0.985f * vy[i] + ay + swirl * x[i];
            const float speed2 = vxn * vxn + vyn * vyn;
            if (speed2 > (float)(kMaxSpeed * kMaxSpeed)) {
                const float inv = (float)(kMaxSpeed / sqrt((double)speed2));
                vxn *= inv;
                vyn *= inv;
            }

            float xn = x[i] + vxn;
            float yn = y[i] + vyn;
            if (xn < 0.02f || xn > 0.98f) {
                vxn = -0.92f * vxn;
                xn = x[i] + vxn;
            }
            if (yn < 0.02f || yn > 0.98f) {
                vyn = -0.92f * vyn;
                yn = y[i] + vyn;
            }

            nx[i] = xn;
            ny[i] = yn;
            nvx[i] = vxn;
            nvy[i] = vyn;
        }

        for (int i = 0; i < kN; ++i) {
            x[i] = nx[i];
            y[i] = ny[i];
            vx[i] = nvx[i];
            vy[i] = nvy[i];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += (double)x[i] + (double)y[i] + 0.25 * ((double)vx[i] + (double)vy[i]);
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 12

int main() {
    constexpr int kW = 12;
    constexpr int kH = 12;
    constexpr int kN = kW * kH;
    constexpr int kSweeps = 48;
    constexpr int kInner = 6;

    static double theta[kN];
    static double volt[kN];
    static double p[kN];
    static double q[kN];
    static double next_theta[kN];
    static double next_volt[kN];

    uint64_t seed = 29;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            const double px = (double)x / (double)(kW - 1) * 2.0 - 1.0;
            const double py = (double)y / (double)(kH - 1) * 2.0 - 1.0;
            theta[idx] = 0.02 * u2bench_rand_sym(&seed);
            volt[idx] = 1.0 + 0.03 * px - 0.02 * py + 0.01 * u2bench_rand_sym(&seed);
            p[idx] = 0.32 * px - 0.24 * py + 0.10 * u2bench_rand_sym(&seed);
            q[idx] = -0.18 * px - 0.10 * py + 0.06 * u2bench_rand_sym(&seed);
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        const double load_wave = 0.022 * sin(0.18 * (double)sweep);
        for (int iter = 0; iter < kInner; ++iter) {
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    const int idx = y * kW + x;

                    if (y == 0 && (x == 0 || x == (kW / 2) || x == (kW - 1))) {
                        next_theta[idx] = 0.0;
                        next_volt[idx] = (x == (kW / 2)) ? 1.05 : 1.03;
                        continue;
                    }

                    const double vi = volt[idx];
                    double p_calc = 0.0;
                    double q_calc = 0.0;
                    double theta_avg = 0.0;
                    double volt_avg = 0.0;
                    double g_sum = 0.0;
                    double b_sum = 0.0;
                    int degree = 0;

                    auto add_edge = [&](int j, int code) {
                        const double gij = 0.08 + 0.012 * (double)(((idx * 7 + j * 3 + code) & 7));
                        const double bij = 0.24 + 0.015 * (double)(((idx * 5 + j * 11 + code * 13) & 7));
                        const double dtheta = theta[j] - theta[idx];
                        const double vj = volt[j];
                        p_calc += vi * vj * (gij * cos(dtheta) + bij * sin(dtheta));
                        q_calc += vi * vj * (gij * sin(dtheta) - bij * cos(dtheta));
                        theta_avg += theta[j];
                        volt_avg += vj;
                        g_sum += gij;
                        b_sum += bij;
                        ++degree;
                    };

                    if (x > 0) {
                        add_edge(idx - 1, 1);
                    }
                    if (x + 1 < kW) {
                        add_edge(idx + 1, 2);
                    }
                    if (y > 0) {
                        add_edge(idx - kW, 3);
                    }
                    if (y + 1 < kH) {
                        add_edge(idx + kW, 4);
                    }

                    const double scale = (((x + y + sweep) & 1) == 0) ? 1.0 + load_wave : 1.0 - load_wave;
                    const double p_spec = p[idx] * scale;
                    const double q_spec = q[idx] * (1.0 - 0.75 * load_wave);
                    const double inv_deg = 1.0 / (double)degree;
                    const double theta_target =
                        theta[idx] + 0.085 * (p_spec - p_calc) / (fabs(vi) * (b_sum + 0.35) + 1e-9);
                    const double volt_target =
                        vi + 0.045 * (q_spec - q_calc) / (g_sum + 0.45) + 0.012 * (1.0 - vi);

                    next_theta[idx] = 0.78 * theta_target + 0.22 * theta_avg * inv_deg;
                    next_volt[idx] =
                        fmin(1.18, fmax(0.82, 0.84 * volt_target + 0.16 * volt_avg * inv_deg));
                }
            }

            for (int i = 0; i < kN; ++i) {
                theta[i] = next_theta[i];
                volt[i] = next_volt[i];
            }
        }

        for (int i = 0; i < kN; ++i) {
            p[i] = 0.996 * p[i] + 0.004 * theta[i] * volt[i];
            q[i] = 0.995 * q[i] + 0.005 * (volt[i] - 1.0);
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += volt[i] * volt[i] + 0.45 * theta[i] * theta[i] + 0.08 * p[i] * theta[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 13

int main() {
    constexpr int kW = 96;
    constexpr int kH = 64;
    constexpr int kN = kW * kH;
    constexpr int kQ = 9;
    constexpr int kSteps = 36;
    constexpr float kOmega = 1.82f;

    static float fa[kN * kQ];
    static float fb[kN * kQ];
    static uint8_t solid[kN];

    static constexpr int kDx[kQ] = {0, 1, 0, -1, 0, 1, -1, -1, 1};
    static constexpr int kDy[kQ] = {0, 0, 1, 0, -1, 1, 1, -1, -1};
    static constexpr int kOpp[kQ] = {0, 3, 4, 1, 2, 7, 8, 5, 6};
    static constexpr float kWgt[kQ] = {
        4.0f / 9.0f,
        1.0f / 9.0f,
        1.0f / 9.0f,
        1.0f / 9.0f,
        1.0f / 9.0f,
        1.0f / 36.0f,
        1.0f / 36.0f,
        1.0f / 36.0f,
        1.0f / 36.0f,
    };

    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            const int ox = x - 24;
            const int oy = y - (kH / 2);
            const bool obstacle = (ox * ox + oy * oy) < 49;
            solid[idx] = (uint8_t)((y == 0 || y == kH - 1 || obstacle) ? 1 : 0);

            const float rho = 1.0f;
            const float profile = 1.0f - 0.72f * (float)(fabs((double)(y - (kH / 2)))) / (float)(kH / 2);
            const float ux = solid[idx] ? 0.0f : 0.045f * profile;
            const float uy = 0.0f;
            const float u2 = ux * ux + uy * uy;

            for (int k = 0; k < kQ; ++k) {
                const float eu = (float)kDx[k] * ux + (float)kDy[k] * uy;
                const float feq = kWgt[k] * rho * (1.0f + 3.0f * eu + 4.5f * eu * eu - 1.5f * u2);
                fa[idx * kQ + k] = feq;
                fb[idx * kQ + k] = feq;
            }
        }
    }

    float* src = fa;
    float* dst = fb;
    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                const int idx = y * kW + x;
                const int base = idx * kQ;

                if (solid[idx]) {
                    for (int k = 0; k < kQ; ++k) {
                        dst[base + k] = src[base + kOpp[k]];
                    }
                    continue;
                }

                float fin[kQ];
                for (int k = 0; k < kQ; ++k) {
                    int sx = x - kDx[k];
                    if (sx < 0) {
                        sx += kW;
                    } else if (sx >= kW) {
                        sx -= kW;
                    }
                    const int sy = y - kDy[k];
                    if (sy < 0 || sy >= kH) {
                        fin[k] = src[base + kOpp[k]];
                        continue;
                    }
                    const int sidx = sy * kW + sx;
                    if (solid[sidx]) {
                        fin[k] = src[base + kOpp[k]];
                    } else {
                        fin[k] = src[sidx * kQ + k];
                    }
                }

                float rho = 0.0f;
                for (int k = 0; k < kQ; ++k) {
                    rho += fin[k];
                }

                float ux =
                    (fin[1] + fin[5] + fin[8] - fin[3] - fin[6] - fin[7]) / (rho + 1e-9f);
                float uy =
                    (fin[2] + fin[5] + fin[6] - fin[4] - fin[7] - fin[8]) / (rho + 1e-9f);
                ux = 0.985f * ux + 0.00045f * (1.0f - 0.7f * (float)y / (float)(kH - 1));
                uy *= 0.992f;

                const float u2 = ux * ux + uy * uy;
                for (int k = 0; k < kQ; ++k) {
                    const float eu = (float)kDx[k] * ux + (float)kDy[k] * uy;
                    const float feq =
                        kWgt[k] * rho * (1.0f + 3.0f * eu + 4.5f * eu * eu - 1.5f * u2);
                    dst[base + k] = fin[k] + kOmega * (feq - fin[k]);
                }
            }
        }

        float* tmp = src;
        src = dst;
        dst = tmp;
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int y = 1; y < kH - 1; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            if (solid[idx]) {
                continue;
            }
            const int base = idx * kQ;
            const float rho = src[base + 0] + src[base + 1] + src[base + 2] + src[base + 3] +
                              src[base + 4] + src[base + 5] + src[base + 6] + src[base + 7] +
                              src[base + 8];
            const float ux =
                (src[base + 1] + src[base + 5] + src[base + 8] - src[base + 3] - src[base + 6] -
                 src[base + 7]) /
                (rho + 1e-9f);
            const float uy =
                (src[base + 2] + src[base + 5] + src[base + 6] - src[base + 4] - src[base + 7] -
                 src[base + 8]) /
                (rho + 1e-9f);
            acc += (double)rho + 0.3 * (double)(ux * ux + uy * uy);
        }
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 14

int main() {
    constexpr int kW = 24;
    constexpr int kH = 24;
    constexpr int kN = kW * kH;
    constexpr int kSteps = 40;
    constexpr int kIters = 8;

    static double ux[kN];
    static double uy[kN];
    static double nx[kN];
    static double ny[kN];
    static double fx[kN];
    static double fy[kN];

    uint64_t seed = 31;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            const double px = (double)x / (double)(kW - 1) * 2.0 - 1.0;
            const double py = (double)y / (double)(kH - 1);
            ux[idx] = 0.004 * u2bench_rand_sym(&seed);
            uy[idx] = -0.010 * py + 0.004 * u2bench_rand_sym(&seed);
            fx[idx] = 0.010 * px + 0.018 * u2bench_rand_sym(&seed);
            fy[idx] = -0.055 * py - 0.010 * (1.0 - px * px) + 0.015 * u2bench_rand_sym(&seed);
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        const double drive = 0.022 * sin(0.14 * (double)step);
        for (int iter = 0; iter < kIters; ++iter) {
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    const int idx = y * kW + x;

                    if (y == 0 && (x == 0 || x == (kW / 2) || x == (kW - 1))) {
                        nx[idx] = 0.0;
                        ny[idx] = 0.0;
                        continue;
                    }

                    double sum_x = 0.0;
                    double sum_y = 0.0;
                    double wsum = 0.0;

                    auto add_nb = [&](int nb, double rx, double ry, double basek) {
                        const double dx = ux[nb] - ux[idx];
                        const double dy = uy[nb] - uy[idx];
                        const double strain = sqrt((dx + rx) * (dx + rx) + (dy + ry) * (dy + ry) + 1e-12);
                        const double k = basek + 0.07 * strain;
                        sum_x += k * (ux[nb] - 0.10 * dx + 0.03 * rx);
                        sum_y += k * (uy[nb] - 0.10 * dy + 0.03 * ry);
                        wsum += k;
                    };

                    if (x > 0) {
                        add_nb(idx - 1, -1.0, 0.0, 0.32);
                    }
                    if (x + 1 < kW) {
                        add_nb(idx + 1, 1.0, 0.0, 0.32);
                    }
                    if (y > 0) {
                        add_nb(idx - kW, 0.0, -1.0, 0.36);
                    }
                    if (y + 1 < kH) {
                        add_nb(idx + kW, 0.0, 1.0, 0.36);
                    }
                    if (x > 0 && y > 0) {
                        add_nb(idx - kW - 1, -1.0, -1.0, 0.11);
                    }
                    if (x + 1 < kW && y > 0) {
                        add_nb(idx - kW + 1, 1.0, -1.0, 0.11);
                    }
                    if (x > 0 && y + 1 < kH) {
                        add_nb(idx + kW - 1, -1.0, 1.0, 0.11);
                    }
                    if (x + 1 < kW && y + 1 < kH) {
                        add_nb(idx + kW + 1, 1.0, 1.0, 0.11);
                    }

                    const double load_x = fx[idx] + drive * (0.20 - 0.015 * (double)y);
                    const double load_y = fy[idx] - 0.010 * drive * (double)(x - (kW / 2));
                    const double tx = (sum_x + load_x) / (wsum + 0.65);
                    const double ty = (sum_y + load_y) / (wsum + 0.72);
                    nx[idx] = 0.76 * tx + 0.24 * ux[idx];
                    ny[idx] = 0.76 * ty + 0.24 * uy[idx];
                }
            }

            for (int i = 0; i < kN; ++i) {
                ux[i] = nx[i];
                uy[i] = ny[i];
            }
        }

        for (int i = 0; i < kN; ++i) {
            fx[i] = 0.996 * fx[i] + 0.004 * ux[i];
            fy[i] = 0.994 * fy[i] + 0.006 * (uy[i] - 0.04);
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += ux[i] * ux[i] + uy[i] * uy[i] + 0.08 * ux[i] * uy[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 15

int main() {
    constexpr int kBatch = 128;
    constexpr int kState = 6;
    constexpr int kCtrl = 2;
    constexpr int kSteps = 96;

    static double state[kBatch][kState];
    static double target[kBatch][kState];
    static double p[kBatch][kState][kState];
    static double pn[kBatch][kState][kState];

    static constexpr double A[kState][kState] = {
        {1.0, 0.08, 0.0, 0.0, 0.0, 0.0},
        {-0.18, 0.92, 0.06, 0.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.08, 0.0, 0.0},
        {0.05, 0.0, -0.22, 0.90, 0.05, 0.0},
        {0.0, 0.0, 0.0, 0.0, 1.0, 0.08},
        {0.0, 0.0, 0.02, 0.0, -0.16, 0.94},
    };
    static constexpr double B[kState][kCtrl] = {
        {0.00, 0.00},
        {0.10, 0.02},
        {0.00, 0.00},
        {0.03, 0.11},
        {0.00, 0.00},
        {0.02, 0.08},
    };
    static constexpr double Qd[kState] = {8.0, 1.8, 7.0, 1.6, 6.2, 1.4};
    static constexpr double Rd[kCtrl] = {0.42, 0.36};

    uint64_t seed = 37;
    for (int s = 0; s < kBatch; ++s) {
        for (int i = 0; i < kState; ++i) {
            state[s][i] = 0.30 * u2bench_rand_sym(&seed) + 0.02 * (double)((s + i) & 7);
            target[s][i] = 0.20 * u2bench_rand_sym(&seed);
            for (int j = 0; j < kState; ++j) {
                p[s][i][j] = (i == j) ? Qd[i] : 0.0;
                pn[s][i][j] = 0.0;
            }
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        const double wave = 0.12 * sin(0.11 * (double)step);
        for (int s = 0; s < kBatch; ++s) {
            double pb[kState][kCtrl];
            double pa[kState][kState];
            double bpa[kCtrl][kState];
            double k_gain[kCtrl][kState];
            double m[kState][kState];
            double atma[kState][kState];
            double err[kState];
            double xnext[kState];

            for (int i = 0; i < kState; ++i) {
                err[i] = state[s][i] - target[s][i];
                for (int a = 0; a < kCtrl; ++a) {
                    double sum = 0.0;
                    for (int j = 0; j < kState; ++j) {
                        sum += p[s][i][j] * B[j][a];
                    }
                    pb[i][a] = sum;
                }
                for (int j = 0; j < kState; ++j) {
                    double sum = 0.0;
                    for (int k = 0; k < kState; ++k) {
                        sum += p[s][i][k] * A[k][j];
                    }
                    pa[i][j] = sum;
                }
            }

            double g00 = Rd[0];
            double g01 = 0.0;
            double g11 = Rd[1];
            for (int i = 0; i < kState; ++i) {
                g00 += B[i][0] * pb[i][0];
                g01 += B[i][0] * pb[i][1];
                g11 += B[i][1] * pb[i][1];
            }
            const double det = g00 * g11 - g01 * g01 + 1e-12;
            const double ig00 = g11 / det;
            const double ig01 = -g01 / det;
            const double ig11 = g00 / det;

            for (int a = 0; a < kCtrl; ++a) {
                for (int j = 0; j < kState; ++j) {
                    double sum = 0.0;
                    for (int i = 0; i < kState; ++i) {
                        sum += B[i][a] * pa[i][j];
                    }
                    bpa[a][j] = sum;
                }
            }

            for (int j = 0; j < kState; ++j) {
                k_gain[0][j] = ig00 * bpa[0][j] + ig01 * bpa[1][j];
                k_gain[1][j] = ig01 * bpa[0][j] + ig11 * bpa[1][j];
            }

            for (int i = 0; i < kState; ++i) {
                for (int j = 0; j < kState; ++j) {
                    m[i][j] = p[s][i][j] - pb[i][0] * k_gain[0][j] - pb[i][1] * k_gain[1][j];
                }
            }

            for (int i = 0; i < kState; ++i) {
                for (int j = 0; j < kState; ++j) {
                    double sum = 0.0;
                    for (int k = 0; k < kState; ++k) {
                        sum += m[i][k] * A[k][j];
                    }
                    atma[i][j] = sum;
                }
            }

            for (int i = 0; i < kState; ++i) {
                for (int j = 0; j < kState; ++j) {
                    double sum = (i == j) ? Qd[i] : 0.0;
                    for (int k = 0; k < kState; ++k) {
                        sum += A[k][i] * atma[k][j];
                    }
                    pn[s][i][j] = sum;
                }
            }

            double u0 = 0.0;
            double u1 = 0.0;
            for (int j = 0; j < kState; ++j) {
                u0 -= k_gain[0][j] * err[j];
                u1 -= k_gain[1][j] * err[j];
            }
            u0 += wave * (0.6 + 0.01 * (double)(s & 7));
            u1 -= wave * (0.4 + 0.02 * (double)(s & 3));

            for (int i = 0; i < kState; ++i) {
                double sum = 0.0;
                for (int j = 0; j < kState; ++j) {
                    sum += A[i][j] * state[s][j];
                }
                sum += B[i][0] * u0 + B[i][1] * u1;
                sum += 0.01 * target[s][i];
                xnext[i] = sum;
            }

            for (int i = 0; i < kState; ++i) {
                state[s][i] = xnext[i];
                target[s][i] = 0.995 * target[s][i] + 0.005 * wave * (double)(i + 1);
                for (int j = 0; j < kState; ++j) {
                    p[s][i][j] = pn[s][i][j];
                }
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int s = 0; s < kBatch; ++s) {
        for (int i = 0; i < kState; ++i) {
            acc += state[s][i] * state[s][i] + 0.015 * p[s][i][i];
        }
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 16

int main() {
    constexpr int kN0 = 128;
    constexpr int kN1 = 64;
    constexpr int kN2 = 32;
    constexpr int kCycles = 24;

    static float u0[kN0 * kN0];
    static float rhs0[kN0 * kN0];
    static float t0buf[kN0 * kN0];
    static float u1[kN1 * kN1];
    static float rhs1[kN1 * kN1];
    static float t1buf[kN1 * kN1];
    static float u2[kN2 * kN2];
    static float rhs2[kN2 * kN2];
    static float t2buf[kN2 * kN2];

    uint64_t seed = 41;
    for (int y = 0; y < kN0; ++y) {
        for (int x = 0; x < kN0; ++x) {
            const int idx = y * kN0 + x;
            const float px = (float)x / (float)(kN0 - 1) * 2.0f - 1.0f;
            const float py = (float)y / (float)(kN0 - 1) * 2.0f - 1.0f;
            float src = 0.0f;
            if ((x > 22 && x < 42) && (y > 28 && y < 46)) {
                src += 1.6f;
            }
            if ((x > 78 && x < 104) && (y > 66 && y < 94)) {
                src -= 1.4f;
            }
            src += 0.18f * px - 0.12f * py + 0.05f * (float)u2bench_rand_sym(&seed);
            rhs0[idx] = src;
            u0[idx] = 0.0f;
            t0buf[idx] = 0.0f;
        }
    }

    auto smooth_level = [](float* u, const float* rhs, float* tmp, int n, int iters, float omega) {
        for (int iter = 0; iter < iters; ++iter) {
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const int idx = y * n + x;
                    if (x == 0 || y == 0 || x == n - 1 || y == n - 1) {
                        tmp[idx] = 0.0f;
                        continue;
                    }
                    const float lap = u[idx - 1] + u[idx + 1] + u[idx - n] + u[idx + n];
                    const float target = 0.25f * (lap + rhs[idx]);
                    tmp[idx] = (1.0f - omega) * u[idx] + omega * target;
                }
            }
            for (int i = 0; i < n * n; ++i) {
                u[i] = tmp[i];
            }
        }
    };

    auto restrict_residual = [](const float* uf, const float* rhsf, float* rhsc, int nf, int nc) {
        for (int y = 0; y < nc; ++y) {
            for (int x = 0; x < nc; ++x) {
                const int idxc = y * nc + x;
                if (x == 0 || y == 0 || x == nc - 1 || y == nc - 1) {
                    rhsc[idxc] = 0.0f;
                    continue;
                }
                const int xf = x * 2;
                const int yf = y * 2;
                const int p0 = yf * nf + xf;
                const int p1 = p0 + 1;
                const int p2 = p0 + nf;
                const int p3 = p2 + 1;
                auto res_at = [&](int p) {
                    return rhsf[p] - (4.0f * uf[p] - uf[p - 1] - uf[p + 1] - uf[p - nf] - uf[p + nf]);
                };
                rhsc[idxc] = 0.25f * (res_at(p0) + res_at(p1) + res_at(p2) + res_at(p3));
            }
        }
    };

    auto prolong_add = [](float* uf, const float* uc, int nf, int nc, float scale) {
        for (int y = 1; y < nf - 1; ++y) {
            for (int x = 1; x < nf - 1; ++x) {
                const int xc = x >> 1;
                const int yc = y >> 1;
                uf[y * nf + x] += scale * uc[yc * nc + xc];
            }
        }
    };

    const uint64_t t0 = u2bench_now_ns();
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        smooth_level(u0, rhs0, t0buf, kN0, 3, 0.78f);

        for (int i = 0; i < kN1 * kN1; ++i) {
            u1[i] = 0.0f;
            t1buf[i] = 0.0f;
        }
        restrict_residual(u0, rhs0, rhs1, kN0, kN1);
        smooth_level(u1, rhs1, t1buf, kN1, 7, 0.82f);

        for (int i = 0; i < kN2 * kN2; ++i) {
            u2[i] = 0.0f;
            t2buf[i] = 0.0f;
        }
        restrict_residual(u1, rhs1, rhs2, kN1, kN2);
        smooth_level(u2, rhs2, t2buf, kN2, 14, 0.88f);

        prolong_add(u1, u2, kN1, kN2, 0.95f);
        smooth_level(u1, rhs1, t1buf, kN1, 4, 0.84f);

        prolong_add(u0, u1, kN0, kN1, 0.90f);
        smooth_level(u0, rhs0, t0buf, kN0, 3, 0.80f);

        const float wave = 0.015f * (float)sin(0.17 * (double)cycle);
        for (int y = 1; y < kN0 - 1; ++y) {
            for (int x = 1; x < kN0 - 1; ++x) {
                rhs0[y * kN0 + x] += wave * (((x + y + cycle) & 7) == 0 ? 1.0f : -0.12f);
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN0 * kN0; ++i) {
        acc += (double)u0[i] * (double)u0[i] + 0.02 * (double)rhs0[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 17

int main() {
    constexpr int kW = 160;
    constexpr int kH = 96;
    constexpr int kN = kW * kH;
    constexpr int kSteps = 60;

    static float ex[kN];
    static float ey[kN];
    static float hz[kN];

    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            const float px = (float)x / (float)(kW - 1);
            const float py = (float)y / (float)(kH - 1);
            ex[idx] = 0.0f;
            ey[idx] = 0.0f;
            hz[idx] = 0.04f * sinf(6.28318f * px) * cosf(3.14159f * py);
        }
    }

    const int sx = kW / 3;
    const int sy = kH / 2;
    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int y = 0; y < kH - 1; ++y) {
            for (int x = 0; x < kW - 1; ++x) {
                const int idx = y * kW + x;
                const float curl =
                    (ey[idx + 1] - ey[idx]) - (ex[idx + kW] - ex[idx]);
                hz[idx] = 0.996f * hz[idx] - 0.48f * curl;
            }
        }

        const int src_idx = sy * kW + sx;
        hz[src_idx] += 0.32f * sinf(0.22f * (float)step);

        for (int y = 1; y < kH; ++y) {
            for (int x = 0; x < kW - 1; ++x) {
                const int idx = y * kW + x;
                ex[idx] = 0.992f * ex[idx] + 0.44f * (hz[idx] - hz[idx - kW]);
            }
        }

        for (int y = 0; y < kH - 1; ++y) {
            for (int x = 1; x < kW; ++x) {
                const int idx = y * kW + x;
                ey[idx] = 0.992f * ey[idx] - 0.44f * (hz[idx] - hz[idx - 1]);
            }
        }

        for (int x = 0; x < kW; ++x) {
            ex[x] = 0.0f;
            ex[(kH - 1) * kW + x] = 0.0f;
            ey[x] = 0.0f;
            ey[(kH - 1) * kW + x] = 0.0f;
        }
        for (int y = 0; y < kH; ++y) {
            ex[y * kW] = 0.0f;
            ex[y * kW + (kW - 1)] = 0.0f;
            ey[y * kW] = 0.0f;
            ey[y * kW + (kW - 1)] = 0.0f;
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += (double)ex[i] * (double)ex[i] + (double)ey[i] * (double)ey[i] +
               0.7 * (double)hz[i] * (double)hz[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 18

int main() {
    constexpr int kBatch = 192;
    constexpr int kLinks = 4;
    constexpr int kSteps = 96;
    static constexpr double kLen[kLinks] = {0.44, 0.33, 0.24, 0.18};

    static double theta[kBatch][kLinks];
    static double target[kBatch][2];

    uint64_t seed = 43;
    for (int b = 0; b < kBatch; ++b) {
        for (int l = 0; l < kLinks; ++l) {
            theta[b][l] = 0.35 * u2bench_rand_sym(&seed);
        }
        target[b][0] = 0.58 + 0.22 * u2bench_rand_sym(&seed);
        target[b][1] = 0.18 + 0.52 * ((double)(b & 31) / 31.0);
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        const double wave = 0.07 * sin(0.11 * (double)step);
        for (int b = 0; b < kBatch; ++b) {
            double ang[kLinks];
            double sx[kLinks];
            double cx[kLinks];
            double acc_ang = 0.0;
            double px = 0.0;
            double py = 0.0;

            for (int l = 0; l < kLinks; ++l) {
                acc_ang += theta[b][l];
                ang[l] = acc_ang;
                sx[l] = sin(acc_ang);
                cx[l] = cos(acc_ang);
                px += kLen[l] * cx[l];
                py += kLen[l] * sx[l];
            }

            const double tx = target[b][0] + wave * (0.5 + 0.04 * (double)(b & 7));
            const double ty = target[b][1] + 0.04 * cos(0.09 * (double)(step + b));
            const double ex = tx - px;
            const double ey = ty - py;

            for (int l = 0; l < kLinks; ++l) {
                double jx = 0.0;
                double jy = 0.0;
                for (int m = l; m < kLinks; ++m) {
                    jx -= kLen[m] * sx[m];
                    jy += kLen[m] * cx[m];
                }
                const double reg = ((l + 1) < kLinks) ? (theta[b][l] - theta[b][l + 1]) : theta[b][l];
                const double grad = jx * ex + jy * ey - 0.035 * theta[b][l] - 0.012 * reg;
                theta[b][l] += (0.14 / (1.0 + 0.3 * (double)l)) * grad;
            }

            target[b][0] = 0.997 * target[b][0] + 0.003 * (0.65 + 0.18 * cos(0.02 * (double)(b + step)));
            target[b][1] = 0.997 * target[b][1] + 0.003 * (0.42 + 0.16 * sin(0.03 * (double)(2 * b + step)));
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int b = 0; b < kBatch; ++b) {
        double px = 0.0;
        double py = 0.0;
        double a = 0.0;
        for (int l = 0; l < kLinks; ++l) {
            a += theta[b][l];
            px += kLen[l] * cos(a);
            py += kLen[l] * sin(a);
            acc += 0.02 * theta[b][l] * theta[b][l];
        }
        const double dx = target[b][0] - px;
        const double dy = target[b][1] - py;
        acc += dx * dx + dy * dy;
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 19

int main() {
    constexpr int kW = 128;
    constexpr int kH = 96;
    constexpr int kN = kW * kH;
    constexpr int kSteps = 64;
    constexpr float kDt = 0.035f;
    constexpr float kG = 0.85f;
    constexpr float kDiff = 0.018f;
    constexpr float kDrag = 0.004f;

    static float h[kN];
    static float hu[kN];
    static float hv[kN];
    static float nh[kN];
    static float nhu[kN];
    static float nhv[kN];

    uint64_t seed = 47;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int idx = y * kW + x;
            const float px = (float)x / (float)(kW - 1) * 2.0f - 1.0f;
            const float py = (float)y / (float)(kH - 1) * 2.0f - 1.0f;
            float bump = 0.0f;
            if ((x > 18 && x < 42) && (y > 26 && y < 48)) {
                bump += 0.32f;
            }
            if ((x > 74 && x < 104) && (y > 56 && y < 78)) {
                bump -= 0.24f;
            }
            h[idx] = 1.0f + bump + 0.05f * px - 0.03f * py + 0.02f * (float)u2bench_rand_sym(&seed);
            hu[idx] = 0.02f * py;
            hv[idx] = -0.02f * px;
            nh[idx] = h[idx];
            nhu[idx] = hu[idx];
            nhv[idx] = hv[idx];
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        const float source = 0.06f * sinf(0.14f * (float)step);
        for (int y = 1; y < kH - 1; ++y) {
            for (int x = 1; x < kW - 1; ++x) {
                const int idx = y * kW + x;
                const float h0 = fmaxf(h[idx], 0.12f);
                const float inv = 1.0f / h0;
                const float u0 = hu[idx] * inv;
                const float v0 = hv[idx] * inv;

                const float lap_h = h[idx - 1] + h[idx + 1] + h[idx - kW] + h[idx + kW] - 4.0f * h0;
                const float lap_hu =
                    hu[idx - 1] + hu[idx + 1] + hu[idx - kW] + hu[idx + kW] - 4.0f * hu[idx];
                const float lap_hv =
                    hv[idx - 1] + hv[idx + 1] + hv[idx - kW] + hv[idx + kW] - 4.0f * hv[idx];

                const float dh =
                    0.5f * ((hu[idx + 1] - hu[idx - 1]) + (hv[idx + kW] - hv[idx - kW]));

                const float flux_x_p =
                    hu[idx + 1] * (hu[idx + 1] / fmaxf(h[idx + 1], 0.12f)) + 0.5f * kG * h[idx + 1] * h[idx + 1];
                const float flux_x_m =
                    hu[idx - 1] * (hu[idx - 1] / fmaxf(h[idx - 1], 0.12f)) + 0.5f * kG * h[idx - 1] * h[idx - 1];
                const float flux_xy_p =
                    hu[idx + kW] * (hv[idx + kW] / fmaxf(h[idx + kW], 0.12f));
                const float flux_xy_m =
                    hu[idx - kW] * (hv[idx - kW] / fmaxf(h[idx - kW], 0.12f));

                const float flux_y_p =
                    hv[idx + kW] * (hv[idx + kW] / fmaxf(h[idx + kW], 0.12f)) + 0.5f * kG * h[idx + kW] * h[idx + kW];
                const float flux_y_m =
                    hv[idx - kW] * (hv[idx - kW] / fmaxf(h[idx - kW], 0.12f)) + 0.5f * kG * h[idx - kW] * h[idx - kW];
                const float flux_yx_p =
                    hv[idx + 1] * (hu[idx + 1] / fmaxf(h[idx + 1], 0.12f));
                const float flux_yx_m =
                    hv[idx - 1] * (hu[idx - 1] / fmaxf(h[idx - 1], 0.12f));

                float hh = h0 - kDt * dh + kDiff * lap_h;
                float hhu =
                    hu[idx] - 0.5f * kDt * ((flux_x_p - flux_x_m) + (flux_xy_p - flux_xy_m)) +
                    kDiff * lap_hu - kDrag * hu[idx];
                float hhv =
                    hv[idx] - 0.5f * kDt * ((flux_y_p - flux_y_m) + (flux_yx_p - flux_yx_m)) +
                    kDiff * lap_hv - kDrag * hv[idx];

                if ((x > 52 && x < 76) && (y > 34 && y < 58)) {
                    hh += 0.01f * source;
                    hhu += 0.004f * source * (float)(x - 64);
                    hhv -= 0.003f * source * (float)(y - 46);
                }

                hh = fminf(2.2f, fmaxf(0.12f, hh));
                const float vel_lim = 0.28f * hh;
                hhu = fminf(vel_lim, fmaxf(-vel_lim, hhu));
                hhv = fminf(vel_lim, fmaxf(-vel_lim, hhv));

                nh[idx] = hh;
                nhu[idx] = hhu;
                nhv[idx] = hhv;
                (void)u0;
                (void)v0;
            }
        }

        for (int x = 0; x < kW; ++x) {
            const int top = x;
            const int topi = kW + x;
            const int bot = (kH - 1) * kW + x;
            const int boti = (kH - 2) * kW + x;
            nh[top] = nh[topi];
            nhu[top] = nhu[topi];
            nhv[top] = -nhv[topi];
            nh[bot] = nh[boti];
            nhu[bot] = nhu[boti];
            nhv[bot] = -nhv[boti];
        }
        for (int y = 0; y < kH; ++y) {
            const int left = y * kW;
            const int lefti = left + 1;
            const int right = y * kW + (kW - 1);
            const int righti = right - 1;
            nh[left] = nh[lefti];
            nhu[left] = -nhu[lefti];
            nhv[left] = nhv[lefti];
            nh[right] = nh[righti];
            nhu[right] = -nhu[righti];
            nhv[right] = nhv[righti];
        }

        for (int i = 0; i < kN; ++i) {
            h[i] = nh[i];
            hu[i] = nhu[i];
            hv[i] = nhv[i];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += (double)h[i] + 0.2 * (double)hu[i] * (double)hu[i] + 0.2 * (double)hv[i] * (double)hv[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 20

int main() {
    constexpr int kN = 768;
    constexpr int kSweeps = 72;

    static double px[kN];
    static double py[kN];
    static double th[kN];

    uint64_t seed = 53;
    for (int i = 0; i < kN; ++i) {
        const double t = (double)i * 0.017;
        px[i] = 0.90 * cos(t) + 0.15 * u2bench_rand_sym(&seed);
        py[i] = 0.90 * sin(t) + 0.15 * u2bench_rand_sym(&seed);
        th[i] = 0.25 * sin(0.5 * t) + 0.20 * u2bench_rand_sym(&seed);
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        const double wave = 0.05 * sin(0.09 * (double)sweep);
        for (int i = 1; i < kN - 1; ++i) {
            const int j = (i + 17) % kN;
            const int k = (i * 7 + sweep * 11) % kN;

            const double t_prev = (double)(i - 1) * 0.017;
            const double t_cur = (double)i * 0.017;
            const double t_next = (double)(i + 1) * 0.017;
            const double dx_prev = 0.90 * cos(t_cur) - 0.90 * cos(t_prev);
            const double dy_prev = 0.90 * sin(t_cur) - 0.90 * sin(t_prev);
            const double dx_next = 0.90 * cos(t_next) - 0.90 * cos(t_cur);
            const double dy_next = 0.90 * sin(t_next) - 0.90 * sin(t_cur);

            const double ex_prev = (px[i] - px[i - 1]) - dx_prev;
            const double ey_prev = (py[i] - py[i - 1]) - dy_prev;
            const double ex_next = (px[i + 1] - px[i]) - dx_next;
            const double ey_next = (py[i + 1] - py[i]) - dy_next;

            const double loop_dx = 0.55 * cos(0.013 * (double)(i + j)) + wave;
            const double loop_dy = 0.55 * sin(0.011 * (double)(i + j)) - 0.6 * wave;
            const double ex_loop = (px[j] - px[i]) - loop_dx;
            const double ey_loop = (py[j] - py[i]) - loop_dy;

            const double anchor_dx = 0.30 * cos(0.015 * (double)k);
            const double anchor_dy = 0.30 * sin(0.012 * (double)k);
            const double ex_anchor = px[k] - anchor_dx;
            const double ey_anchor = py[k] - anchor_dy;

            const double gx = 0.48 * ex_prev - 0.44 * ex_next + 0.09 * ex_loop - 0.05 * ex_anchor;
            const double gy = 0.48 * ey_prev - 0.44 * ey_next + 0.09 * ey_loop - 0.05 * ey_anchor;

            const double th_prev = atan2(dy_prev, dx_prev);
            const double th_next = atan2(dy_next, dx_next);
            const double th_loop = atan2(loop_dy, loop_dx);
            double eth = (th[i] - th_prev) + (th[i] - th_next);
            eth += 0.18 * (th[i] - th_loop);
            eth += 0.07 * (th[i] - th[k]);

            px[i] -= 0.065 * gx;
            py[i] -= 0.065 * gy;
            th[i] -= 0.055 * eth;
        }

        for (int i = 0; i < kN; ++i) {
            px[i] = 0.998 * px[i];
            py[i] = 0.998 * py[i];
            th[i] = 0.996 * th[i];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += px[i] * px[i] + py[i] * py[i] + 0.15 * th[i] * th[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 21

int main() {
    constexpr int kTracks = 32;
    constexpr int kParticles = 256;
    constexpr int kSteps = 72;

    static float px[kTracks][kParticles];
    static float py[kTracks][kParticles];
    static float vx[kTracks][kParticles];
    static float vy[kTracks][kParticles];
    static float w[kTracks][kParticles];
    static float cdf[kParticles];
    static float nx[kParticles];
    static float ny[kParticles];
    static float nvx[kParticles];
    static float nvy[kParticles];

    float tx[kTracks];
    float ty[kTracks];
    float tvx[kTracks];
    float tvy[kTracks];

    uint64_t seed = 59;
    for (int t = 0; t < kTracks; ++t) {
        tx[t] = 0.20f + 0.02f * (float)(t & 7);
        ty[t] = 0.30f + 0.015f * (float)((t * 3) & 7);
        tvx[t] = 0.008f + 0.001f * (float)(t & 3);
        tvy[t] = -0.006f + 0.001f * (float)((t + 1) & 3);
        for (int p = 0; p < kParticles; ++p) {
            px[t][p] = tx[t] + 0.08f * (float)u2bench_rand_sym(&seed);
            py[t][p] = ty[t] + 0.08f * (float)u2bench_rand_sym(&seed);
            vx[t][p] = tvx[t] + 0.01f * (float)u2bench_rand_sym(&seed);
            vy[t][p] = tvy[t] + 0.01f * (float)u2bench_rand_sym(&seed);
            w[t][p] = 1.0f / (float)kParticles;
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        const float meas_wave = 0.018f * sinf(0.14f * (float)step);
        for (int t = 0; t < kTracks; ++t) {
            tx[t] += tvx[t] + 0.002f * sinf(0.05f * (float)(step + t));
            ty[t] += tvy[t] + 0.002f * cosf(0.04f * (float)(step + 2 * t));
            const float mx = tx[t] + meas_wave + 0.01f * (float)u2bench_rand_sym(&seed);
            const float my = ty[t] - 0.7f * meas_wave + 0.01f * (float)u2bench_rand_sym(&seed);

            float wsum = 0.0f;
            for (int p = 0; p < kParticles; ++p) {
                const float ax = 0.0015f * (float)u2bench_rand_sym(&seed);
                const float ay = 0.0015f * (float)u2bench_rand_sym(&seed);
                vx[t][p] = 0.992f * vx[t][p] + ax;
                vy[t][p] = 0.992f * vy[t][p] + ay;
                px[t][p] += vx[t][p];
                py[t][p] += vy[t][p];

                const float dx = mx - px[t][p];
                const float dy = my - py[t][p];
                const float d2 = dx * dx + dy * dy;
                const float likelihood = 1.0f / (0.0025f + d2);
                w[t][p] = 0.15f * w[t][p] + 0.85f * likelihood;
                wsum += w[t][p];
            }

            float inv_wsum = 1.0f / (wsum + 1e-12f);
            float accum = 0.0f;
            for (int p = 0; p < kParticles; ++p) {
                w[t][p] *= inv_wsum;
                accum += w[t][p];
                cdf[p] = accum;
            }

            const float u0 = 0.5f / (float)kParticles;
            int idx = 0;
            for (int m = 0; m < kParticles; ++m) {
                const float u = u0 + (float)m / (float)kParticles;
                while (idx + 1 < kParticles && cdf[idx] < u) {
                    ++idx;
                }
                nx[m] = px[t][idx] + 0.004f * (float)u2bench_rand_sym(&seed);
                ny[m] = py[t][idx] + 0.004f * (float)u2bench_rand_sym(&seed);
                nvx[m] = vx[t][idx];
                nvy[m] = vy[t][idx];
            }

            for (int p = 0; p < kParticles; ++p) {
                px[t][p] = nx[p];
                py[t][p] = ny[p];
                vx[t][p] = nvx[p];
                vy[t][p] = nvy[p];
                w[t][p] = 1.0f / (float)kParticles;
            }

            tx[t] = 0.999f * tx[t] + 0.001f * mx;
            ty[t] = 0.999f * ty[t] + 0.001f * my;
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int t = 0; t < kTracks; ++t) {
        for (int p = 0; p < kParticles; ++p) {
            acc += (double)px[t][p] + (double)py[t][p] + 0.15 * ((double)vx[t][p] + (double)vy[t][p]);
        }
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 22

static inline void bicgstab_apply(const double* x, double* y) {
    constexpr int kN = 96;
    for (int i = 0; i < kN; ++i) {
        double sum = (5.0 + 0.04 * (double)(i & 7)) * x[i];
        for (int j = 0; j < kN; ++j) {
            if (j == i) {
                continue;
            }
            const int mix = (i * 17 + j * 11 + ((i ^ j) & 15)) & 31;
            const double coeff = 0.0025 * (double)(mix - 15);
            sum += coeff * x[j];
        }
        const int l = (i + kN - 1) % kN;
        const int r = (i + 1) % kN;
        const int s = (i * 7 + 13) % kN;
        sum += 0.08 * x[l] - 0.06 * x[r] + 0.035 * x[s];
        y[i] = sum;
    }
}

int main() {
    constexpr int kN = 96;
    constexpr int kIters = 56;

    static double x[kN];
    static double b[kN];
    static double r[kN];
    static double r0[kN];
    static double p[kN];
    static double v[kN];
    static double s[kN];
    static double t[kN];
    static double phat[kN];
    static double shat[kN];
    static double diag_inv[kN];

    for (int i = 0; i < kN; ++i) {
        const double ti = (double)i * 0.07;
        x[i] = 0.0;
        b[i] = 0.8 * sin(ti) + 0.35 * cos(0.6 * ti) + 0.10 * sin(0.11 * (double)(i * i));
        r[i] = b[i];
        r0[i] = r[i];
        p[i] = 0.0;
        v[i] = 0.0;
        s[i] = 0.0;
        t[i] = 0.0;
        phat[i] = 0.0;
        shat[i] = 0.0;
        diag_inv[i] = 1.0 / (5.0 + 0.04 * (double)(i & 7));
    }

    double rho_prev = 1.0;
    double alpha = 1.0;
    double omega = 1.0;

    const uint64_t t0 = u2bench_now_ns();
    for (int iter = 0; iter < kIters; ++iter) {
        double rho = 0.0;
        for (int i = 0; i < kN; ++i) {
            rho += r0[i] * r[i];
        }
        if (fabs(rho) < 1e-18) {
            break;
        }

        if (iter == 0) {
            for (int i = 0; i < kN; ++i) {
                p[i] = r[i];
            }
        } else {
            const double beta = (rho / rho_prev) * (alpha / omega);
            for (int i = 0; i < kN; ++i) {
                p[i] = r[i] + beta * (p[i] - omega * v[i]);
            }
        }

        for (int i = 0; i < kN; ++i) {
            phat[i] = diag_inv[i] * p[i];
        }
        bicgstab_apply(phat, v);

        double denom = 0.0;
        for (int i = 0; i < kN; ++i) {
            denom += r0[i] * v[i];
        }
        alpha = rho / ((fabs(denom) < 1e-18) ? 1e-18 : denom);

        double snorm = 0.0;
        for (int i = 0; i < kN; ++i) {
            s[i] = r[i] - alpha * v[i];
            snorm += s[i] * s[i];
        }

        if (snorm < 1e-16) {
            for (int i = 0; i < kN; ++i) {
                x[i] += alpha * phat[i];
            }
            break;
        }

        for (int i = 0; i < kN; ++i) {
            shat[i] = diag_inv[i] * s[i];
        }
        bicgstab_apply(shat, t);

        double tt = 0.0;
        double ts = 0.0;
        for (int i = 0; i < kN; ++i) {
            tt += t[i] * t[i];
            ts += t[i] * s[i];
        }
        omega = ts / ((fabs(tt) < 1e-18) ? 1e-18 : tt);

        for (int i = 0; i < kN; ++i) {
            x[i] += alpha * phat[i] + omega * shat[i];
            r[i] = s[i] - omega * t[i];
        }
        rho_prev = rho;
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += x[i] * x[i] + 0.07 * r[i] * r[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 23

int main() {
    constexpr int kSystems = 48;
    constexpr int kStateDim = 4;
    constexpr int kHorizon = 12;
    constexpr int kSweeps = 120;

    static double x0[kSystems][kStateDim];
    static double ref[kSystems][kStateDim];
    static double u[kSystems][kHorizon];
    static double xpred[kHorizon + 1][kStateDim];
    static double lambda[kHorizon + 1][kStateDim];

    uint64_t seed = 71;
    for (int s = 0; s < kSystems; ++s) {
        for (int i = 0; i < kStateDim; ++i) {
            x0[s][i] = 0.15 * (double)(i + 1) + 0.08 * u2bench_rand_sym(&seed);
            ref[s][i] = 0.30 * sin(0.2 * (double)(s + i)) + 0.04 * u2bench_rand_sym(&seed);
        }
        for (int t = 0; t < kHorizon; ++t) {
            u[s][t] = 0.05 * u2bench_rand_sym(&seed);
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        for (int s = 0; s < kSystems; ++s) {
            for (int i = 0; i < kStateDim; ++i) {
                xpred[0][i] = x0[s][i];
            }

            for (int t = 0; t < kHorizon; ++t) {
                const double ut = u[s][t];
                const double bias = 0.03 * sin(0.05 * (double)(sweep + t + s));
                const double* xp = xpred[t];
                double* xn = xpred[t + 1];

                xn[0] = 0.92 * xp[0] + 0.11 * xp[1] + 0.08 * ut + bias;
                xn[1] = -0.07 * xp[0] + 0.95 * xp[1] + 0.06 * xp[2] + 0.05 * ut;
                xn[2] = 0.05 * xp[1] + 0.91 * xp[2] + 0.10 * xp[3] + 0.04 * ut - 0.7 * bias;
                xn[3] = 0.03 * xp[0] - 0.08 * xp[2] + 0.97 * xp[3] + 0.03 * ut;
            }

            for (int i = 0; i < kStateDim; ++i) {
                lambda[kHorizon][i] = 2.2 * (xpred[kHorizon][i] - ref[s][i]);
            }

            for (int t = kHorizon - 1; t >= 0; --t) {
                const double* xn = xpred[t + 1];
                const double* ln = lambda[t + 1];
                double* lc = lambda[t];

                lc[0] = 1.5 * (xn[0] - ref[s][0]) + 0.92 * ln[0] - 0.07 * ln[1] + 0.03 * ln[3];
                lc[1] = 1.4 * (xn[1] - ref[s][1]) + 0.11 * ln[0] + 0.95 * ln[1] + 0.05 * ln[2];
                lc[2] = 1.3 * (xn[2] - ref[s][2]) + 0.06 * ln[1] + 0.91 * ln[2] - 0.08 * ln[3];
                lc[3] = 1.2 * (xn[3] - ref[s][3]) + 0.10 * ln[2] + 0.97 * ln[3];

                const double grad_u = 0.14 * u[s][t] + 0.08 * ln[0] + 0.05 * ln[1] + 0.04 * ln[2] + 0.03 * ln[3];
                double next_u = u[s][t] - 0.22 * grad_u;
                next_u = fmin(0.85, fmax(-0.85, next_u));
                u[s][t] = 0.96 * u[s][t] + 0.04 * next_u;
            }

            for (int i = 0; i < kStateDim; ++i) {
                x0[s][i] = 0.995 * x0[s][i] + 0.005 * xpred[1][i];
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int s = 0; s < kSystems; ++s) {
        for (int i = 0; i < kStateDim; ++i) {
            acc += x0[s][i] * x0[s][i] + 0.2 * ref[s][i] * ref[s][i];
        }
        for (int t = 0; t < kHorizon; ++t) {
            acc += 0.08 * u[s][t] * u[s][t];
        }
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 24

static inline void gmres_apply(const double* x, double* y) {
    constexpr int kN = 88;
    for (int i = 0; i < kN; ++i) {
        double sum = (4.8 + 0.06 * (double)(i & 7)) * x[i];
        const int l = (i + kN - 1) % kN;
        const int r = (i + 1) % kN;
        const int j1 = (i * 9 + 7) % kN;
        const int j2 = (i * 13 + 11) % kN;
        sum += 0.09 * x[l] - 0.05 * x[r] + 0.04 * x[j1] - 0.03 * x[j2];
        for (int j = 0; j < kN; ++j) {
            if (j == i) {
                continue;
            }
            const int mix = (i * 19 + j * 5 + ((i ^ j) & 7)) & 15;
            sum += 0.0018 * (double)(mix - 7) * x[j];
        }
        y[i] = sum;
    }
}

int main() {
    constexpr int kN = 88;
    constexpr int kRestart = 10;
    constexpr int kCycles = 44;

    static double x[kN];
    static double b[kN];
    static double r[kN];
    static double w[kN];
    static double v[kRestart + 1][kN];
    static double h[kRestart + 1][kRestart];
    static double cs[kRestart];
    static double sn[kRestart];
    static double g[kRestart + 1];
    static double ycoef[kRestart];

    for (int i = 0; i < kN; ++i) {
        x[i] = 0.0;
        b[i] = 0.7 * sin(0.05 * (double)i) + 0.22 * cos(0.12 * (double)i) + 0.08 * sin(0.01 * (double)(i * i));
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        gmres_apply(x, w);
        double beta2 = 0.0;
        for (int i = 0; i < kN; ++i) {
            r[i] = b[i] - w[i];
            beta2 += r[i] * r[i];
        }
        const double beta = sqrt(beta2);
        if (beta < 1e-14) {
            break;
        }

        for (int i = 0; i < kN; ++i) {
            v[0][i] = r[i] / beta;
        }
        for (int i = 0; i <= kRestart; ++i) {
            g[i] = 0.0;
            if (i < kRestart) {
                cs[i] = 1.0;
                sn[i] = 0.0;
                ycoef[i] = 0.0;
            }
            for (int j = 0; j < kRestart; ++j) {
                h[i][j] = 0.0;
            }
        }
        g[0] = beta;

        int m = 0;
        for (; m < kRestart; ++m) {
            gmres_apply(v[m], w);
            for (int i = 0; i <= m; ++i) {
                double hij = 0.0;
                for (int k = 0; k < kN; ++k) {
                    hij += w[k] * v[i][k];
                }
                h[i][m] = hij;
                for (int k = 0; k < kN; ++k) {
                    w[k] -= hij * v[i][k];
                }
            }

            double normw2 = 0.0;
            for (int k = 0; k < kN; ++k) {
                normw2 += w[k] * w[k];
            }
            h[m + 1][m] = sqrt(normw2);
            if (h[m + 1][m] > 1e-14) {
                for (int k = 0; k < kN; ++k) {
                    v[m + 1][k] = w[k] / h[m + 1][m];
                }
            }

            for (int i = 0; i < m; ++i) {
                const double t0h = cs[i] * h[i][m] + sn[i] * h[i + 1][m];
                const double t1h = -sn[i] * h[i][m] + cs[i] * h[i + 1][m];
                h[i][m] = t0h;
                h[i + 1][m] = t1h;
            }

            const double denom = hypot(h[m][m], h[m + 1][m]);
            cs[m] = h[m][m] / ((denom < 1e-18) ? 1e-18 : denom);
            sn[m] = h[m + 1][m] / ((denom < 1e-18) ? 1e-18 : denom);
            h[m][m] = cs[m] * h[m][m] + sn[m] * h[m + 1][m];
            h[m + 1][m] = 0.0;

            const double gm = g[m];
            g[m] = cs[m] * gm;
            g[m + 1] = -sn[m] * gm;

            if (fabs(g[m + 1]) < 1e-12) {
                ++m;
                break;
            }
        }

        const int used = (m < 1) ? 1 : ((m > kRestart) ? kRestart : m);
        for (int i = used - 1; i >= 0; --i) {
            double sum = g[i];
            for (int j = i + 1; j < used; ++j) {
                sum -= h[i][j] * ycoef[j];
            }
            ycoef[i] = sum / ((fabs(h[i][i]) < 1e-18) ? 1e-18 : h[i][i]);
        }
        for (int j = 0; j < used; ++j) {
            for (int i = 0; i < kN; ++i) {
                x[i] += ycoef[j] * v[j][i];
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kN; ++i) {
        acc += x[i] * x[i];
    }
    gmres_apply(x, w);
    for (int i = 0; i < kN; ++i) {
        const double rr = b[i] - w[i];
        acc += 0.09 * rr * rr;
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 25

int main() {
    constexpr int kBus = 96;
    constexpr int kSweeps = 96;

    static double theta[kBus];
    static double gen[kBus];
    static double demand[kBus];
    static double gmin[kBus];
    static double gmax[kBus];

    uint64_t seed = 83;
    for (int i = 0; i < kBus; ++i) {
        theta[i] = 0.02 * u2bench_rand_sym(&seed);
        demand[i] = 0.55 + 0.08 * sin(0.09 * (double)i) + 0.05 * u2bench_rand_sym(&seed);
        gmin[i] = 0.05 + 0.01 * (double)(i & 3);
        gmax[i] = 1.10 + 0.05 * (double)(i & 7);
        gen[i] = 0.65 + 0.06 * cos(0.07 * (double)i) + 0.04 * u2bench_rand_sym(&seed);
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        double mean_theta = 0.0;
        for (int i = 0; i < kBus; ++i) {
            const int l = (i + kBus - 1) % kBus;
            const int r = (i + 1) % kBus;
            const int c1 = (i * 5 + 13) % kBus;
            const int c2 = (i * 9 + 7) % kBus;

            const double b_lr = 2.2 + 0.10 * (double)(i & 7);
            const double b_c1 = 0.8 + 0.04 * (double)(i & 3);
            const double b_c2 = 0.6 + 0.03 * (double)((i + 2) & 3);

            const double flow_lr = b_lr * (theta[i] - theta[r]);
            const double flow_ll = b_lr * (theta[l] - theta[i]);
            const double flow_c1 = b_c1 * (theta[i] - theta[c1]);
            const double flow_c2 = b_c2 * (theta[i] - theta[c2]);

            const double injection = gen[i] - demand[i] - flow_lr + flow_ll - flow_c1 - flow_c2;
            double grad_g = 0.18 * gen[i] + 0.05 * (double)(i & 7) + 1.35 * injection;
            double grad_t = -1.55 * injection;

            const double lim1 = 0.95 + 0.03 * (double)(i & 3);
            if (fabs(flow_lr) > lim1) {
                grad_t += 0.42 * ((flow_lr > 0.0) ? 1.0 : -1.0) * b_lr;
            }
            const double lim2 = 0.72 + 0.02 * (double)((i + 1) & 3);
            if (fabs(flow_c1) > lim2) {
                grad_t += 0.30 * ((flow_c1 > 0.0) ? 1.0 : -1.0) * b_c1;
            }
            if (fabs(flow_c2) > lim2) {
                grad_t += 0.24 * ((flow_c2 > 0.0) ? 1.0 : -1.0) * b_c2;
            }

            gen[i] -= 0.035 * grad_g;
            gen[i] = fmin(gmax[i], fmax(gmin[i], gen[i]));
            theta[i] -= 0.015 * grad_t;
            mean_theta += theta[i];
        }

        mean_theta /= (double)kBus;
        for (int i = 0; i < kBus; ++i) {
            theta[i] -= mean_theta;
            demand[i] = 0.998 * demand[i] + 0.002 * (0.60 + 0.05 * sin(0.07 * (double)(sweep + i)));
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kBus; ++i) {
        acc += 0.35 * theta[i] * theta[i] + gen[i] * gen[i] + 0.20 * demand[i] * demand[i];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 26

int main() {
    constexpr int kCam = 48;
    constexpr int kPt = 192;
    constexpr int kObs = 768;
    constexpr int kSweeps = 60;

    static double cam[kCam][6];
    static double pt[kPt][3];
    static double gcam[kCam][6];
    static double gpt[kPt][3];

    uint64_t seed = 97;
    for (int c = 0; c < kCam; ++c) {
        const double ang = 0.13 * (double)c;
        cam[c][0] = 0.04 * u2bench_rand_sym(&seed);
        cam[c][1] = 0.04 * u2bench_rand_sym(&seed);
        cam[c][2] = 0.04 * u2bench_rand_sym(&seed);
        cam[c][3] = 1.2 * cos(ang) + 0.05 * u2bench_rand_sym(&seed);
        cam[c][4] = 1.2 * sin(ang) + 0.05 * u2bench_rand_sym(&seed);
        cam[c][5] = 1.4 + 0.08 * sin(0.5 * ang) + 0.03 * u2bench_rand_sym(&seed);
    }
    for (int p = 0; p < kPt; ++p) {
        const double ax = 0.09 * (double)(p % 24);
        const double ay = 0.11 * (double)((p / 24) % 8);
        pt[p][0] = -1.0 + 0.09 * cos(ax) + 0.12 * u2bench_rand_sym(&seed);
        pt[p][1] = -0.8 + 0.10 * sin(ay) + 0.12 * u2bench_rand_sym(&seed);
        pt[p][2] = 0.2 + 0.06 * sin(0.03 * (double)p) + 0.08 * u2bench_rand_sym(&seed);
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        for (int c = 0; c < kCam; ++c) {
            for (int j = 0; j < 6; ++j) {
                gcam[c][j] = 0.0;
            }
        }
        for (int p = 0; p < kPt; ++p) {
            for (int j = 0; j < 3; ++j) {
                gpt[p][j] = 0.0;
            }
        }

        for (int o = 0; o < kObs; ++o) {
            const int c = (o * 7 + 3) % kCam;
            const int p = (o * 13 + 5) % kPt;

            const double rx = cam[c][0];
            const double ry = cam[c][1];
            const double rz = cam[c][2];
            const double tx = cam[c][3];
            const double ty = cam[c][4];
            const double tz = cam[c][5];
            const double px = pt[p][0];
            const double py = pt[p][1];
            const double pz = pt[p][2];

            const double X = px + ry * pz - rz * py + tx;
            const double Y = py + rz * px - rx * pz + ty;
            const double Z = pz + rx * py - ry * px + tz + 2.8;
            const double Zs = (Z < 0.45) ? 0.45 : Z;
            const double invZ = 1.0 / Zs;
            const double u = X * invZ;
            const double v = Y * invZ;

            const double tu = 0.18 * sin(0.017 * (double)o) + 0.03 * cos(0.05 * (double)(c + p)) + 0.01 * sin(0.09 * (double)sweep);
            const double tv = -0.16 * cos(0.015 * (double)o) + 0.02 * sin(0.04 * (double)(c - p)) - 0.01 * cos(0.08 * (double)sweep);
            const double ru = u - tu;
            const double rv = v - tv;
            const double w = 1.0 / (1.0 + 0.8 * (ru * ru + rv * rv));

            const double du_dX = w * invZ;
            const double du_dZ = -w * X * invZ * invZ;
            const double dv_dY = w * invZ;
            const double dv_dZ = -w * Y * invZ * invZ;

            gcam[c][3] += ru * du_dX;
            gcam[c][4] += rv * dv_dY;
            gcam[c][5] += ru * du_dZ + rv * dv_dZ;

            const double dud_rx = du_dZ * py;
            const double dvd_rx = -dv_dY * pz + dv_dZ * py;
            const double dud_ry = du_dX * pz - du_dZ * px;
            const double dvd_ry = -dv_dZ * px;
            const double dud_rz = -du_dX * py;
            const double dvd_rz = dv_dY * px;

            gcam[c][0] += ru * dud_rx + rv * dvd_rx;
            gcam[c][1] += ru * dud_ry + rv * dvd_ry;
            gcam[c][2] += ru * dud_rz + rv * dvd_rz;

            const double dud_px = du_dX - du_dZ * ry;
            const double dvd_px = dv_dY * rz - dv_dZ * ry;
            const double dud_py = -du_dX * rz + du_dZ * rx;
            const double dvd_py = dv_dY + dv_dZ * rx;
            const double dud_pz = du_dX * ry + du_dZ;
            const double dvd_pz = -dv_dY * rx + dv_dZ;

            gpt[p][0] += ru * dud_px + rv * dvd_px;
            gpt[p][1] += ru * dud_py + rv * dvd_py;
            gpt[p][2] += ru * dud_pz + rv * dvd_pz;
        }

        for (int c = 0; c < kCam; ++c) {
            for (int j = 0; j < 6; ++j) {
                gcam[c][j] += 0.01 * cam[c][j];
                cam[c][j] -= 0.018 * gcam[c][j];
            }
            cam[c][5] = fmin(2.4, fmax(0.8, cam[c][5]));
        }
        for (int p = 0; p < kPt; ++p) {
            for (int j = 0; j < 3; ++j) {
                gpt[p][j] += 0.008 * pt[p][j];
                pt[p][j] -= 0.024 * gpt[p][j];
            }
            pt[p][2] = fmin(0.9, fmax(-0.4, pt[p][2]));
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int c = 0; c < kCam; ++c) {
        for (int j = 0; j < 6; ++j) {
            acc += cam[c][j] * cam[c][j];
        }
    }
    for (int p = 0; p < kPt; ++p) {
        for (int j = 0; j < 3; ++j) {
            acc += 0.4 * pt[p][j] * pt[p][j];
        }
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 27

int main() {
    constexpr int kTraj = 64;
    constexpr int kH = 18;
    constexpr int kSweeps = 96;

    static double x0[kTraj][4];
    static double goal[kTraj][2];
    static double u[kTraj][kH][2];
    static double xpred[kH + 1][4];
    static double lambda[kH + 1][4];

    uint64_t seed = 101;
    for (int tr = 0; tr < kTraj; ++tr) {
        x0[tr][0] = -0.6 + 0.03 * (double)(tr & 7) + 0.04 * u2bench_rand_sym(&seed);
        x0[tr][1] = -0.5 + 0.02 * (double)((tr * 3) & 7) + 0.04 * u2bench_rand_sym(&seed);
        x0[tr][2] = 0.05 * u2bench_rand_sym(&seed);
        x0[tr][3] = 0.05 * u2bench_rand_sym(&seed);
        goal[tr][0] = 0.7 - 0.02 * (double)(tr & 7);
        goal[tr][1] = 0.6 - 0.03 * (double)((tr * 5) & 7);
        for (int t = 0; t < kH; ++t) {
            u[tr][t][0] = 0.08 * u2bench_rand_sym(&seed);
            u[tr][t][1] = 0.08 * u2bench_rand_sym(&seed);
        }
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        for (int tr = 0; tr < kTraj; ++tr) {
            for (int j = 0; j < 4; ++j) {
                xpred[0][j] = x0[tr][j];
            }

            for (int t = 0; t < kH; ++t) {
                const double ux = u[tr][t][0];
                const double uy = u[tr][t][1];
                const double* xp = xpred[t];
                double* xn = xpred[t + 1];

                xn[0] = xp[0] + 0.18 * xp[2] + 0.05 * ux;
                xn[1] = xp[1] + 0.18 * xp[3] + 0.05 * uy;
                xn[2] = 0.91 * xp[2] + 0.13 * ux - 0.03 * xp[0] - 0.02 * xp[2] * fabs(xp[2]) + 0.01 * sin(0.04 * (double)(sweep + tr + t));
                xn[3] = 0.92 * xp[3] + 0.13 * uy - 0.03 * xp[1] - 0.02 * xp[3] * fabs(xp[3]) + 0.01 * cos(0.03 * (double)(sweep + 2 * tr + t));
            }

            lambda[kH][0] = 2.4 * (xpred[kH][0] - goal[tr][0]);
            lambda[kH][1] = 2.4 * (xpred[kH][1] - goal[tr][1]);
            lambda[kH][2] = 0.7 * xpred[kH][2];
            lambda[kH][3] = 0.7 * xpred[kH][3];

            for (int t = kH - 1; t >= 0; --t) {
                const double* xn = xpred[t + 1];
                const double* xp = xpred[t];
                const double* ln = lambda[t + 1];
                double* lc = lambda[t];

                const double gx = goal[tr][0] * (double)(t + 1) / (double)kH;
                const double gy = goal[tr][1] * (double)(t + 1) / (double)kH;
                const double ox = 0.18 * sin(0.08 * (double)(sweep + t)) + 0.12 * cos(0.05 * (double)(tr + t));
                const double oy = -0.16 * cos(0.07 * (double)(sweep + t)) + 0.10 * sin(0.04 * (double)(tr + 2 * t));
                const double dx = xn[0] - ox;
                const double dy = xn[1] - oy;
                const double d2 = dx * dx + dy * dy + 0.03;
                const double obs_gx = -0.04 * dx / (d2 * d2);
                const double obs_gy = -0.04 * dy / (d2 * d2);

                lc[0] = 0.6 * (xn[0] - gx) + obs_gx + ln[0] - 0.03 * ln[2];
                lc[1] = 0.6 * (xn[1] - gy) + obs_gy + ln[1] - 0.03 * ln[3];
                lc[2] = 0.12 * xn[2] + 0.18 * ln[0] + (0.91 - 0.04 * fabs(xp[2])) * ln[2];
                lc[3] = 0.12 * xn[3] + 0.18 * ln[1] + (0.92 - 0.04 * fabs(xp[3])) * ln[3];

                double gux = 0.16 * u[tr][t][0] + 0.05 * ln[0] + 0.13 * ln[2];
                double guy = 0.16 * u[tr][t][1] + 0.05 * ln[1] + 0.13 * ln[3];

                if (fabs(xn[0]) > 1.1) {
                    gux += 0.18 * ((xn[0] > 0.0) ? 1.0 : -1.0);
                }
                if (fabs(xn[1]) > 1.1) {
                    guy += 0.18 * ((xn[1] > 0.0) ? 1.0 : -1.0);
                }

                double dux = -0.22 * gux / (1.0 + 0.15 * fabs(gux));
                double duy = -0.22 * guy / (1.0 + 0.15 * fabs(guy));
                u[tr][t][0] = fmin(1.0, fmax(-1.0, 0.96 * u[tr][t][0] + 0.04 * (u[tr][t][0] + dux)));
                u[tr][t][1] = fmin(1.0, fmax(-1.0, 0.96 * u[tr][t][1] + 0.04 * (u[tr][t][1] + duy)));
            }

            for (int j = 0; j < 4; ++j) {
                x0[tr][j] = 0.994 * x0[tr][j] + 0.006 * xpred[1][j];
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int tr = 0; tr < kTraj; ++tr) {
        for (int j = 0; j < 4; ++j) {
            acc += x0[tr][j] * x0[tr][j];
        }
        for (int t = 0; t < kH; ++t) {
            acc += 0.12 * u[tr][t][0] * u[tr][t][0] + 0.12 * u[tr][t][1] * u[tr][t][1];
        }
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 28

int main() {
    constexpr int kPose = 384;
    constexpr int kLand = 256;
    constexpr int kObs = 1024;
    constexpr int kSweeps = 64;

    static double px[kPose];
    static double py[kPose];
    static double th[kPose];
    static double lx[kLand];
    static double ly[kLand];
    static double gpx[kPose];
    static double gpy[kPose];
    static double gth[kPose];
    static double glx[kLand];
    static double gly[kLand];

    uint64_t seed = 109;
    for (int i = 0; i < kPose; ++i) {
        const double t = 0.027 * (double)i;
        px[i] = 1.4 * cos(t) + 0.05 * u2bench_rand_sym(&seed);
        py[i] = 1.2 * sin(t) + 0.05 * u2bench_rand_sym(&seed);
        th[i] = 0.18 * sin(0.6 * t) + 0.04 * u2bench_rand_sym(&seed);
    }
    for (int l = 0; l < kLand; ++l) {
        const double ax = 0.14 * (double)(l % 32);
        const double ay = 0.18 * (double)(l / 32);
        lx[l] = -1.6 + 0.11 * cos(ax) + 0.16 * u2bench_rand_sym(&seed);
        ly[l] = -1.1 + 0.13 * sin(ay) + 0.16 * u2bench_rand_sym(&seed);
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int sweep = 0; sweep < kSweeps; ++sweep) {
        for (int i = 0; i < kPose; ++i) {
            gpx[i] = 0.0;
            gpy[i] = 0.0;
            gth[i] = 0.0;
        }
        for (int l = 0; l < kLand; ++l) {
            glx[l] = 0.0;
            gly[l] = 0.0;
        }

        for (int i = 1; i < kPose; ++i) {
            const double ex = 0.035 * cos(0.013 * (double)i);
            const double ey = 0.032 * sin(0.011 * (double)i);
            const double eth = 0.004 * sin(0.021 * (double)i);
            const double rx = (px[i] - px[i - 1]) - ex;
            const double ry = (py[i] - py[i - 1]) - ey;
            const double rth = (th[i] - th[i - 1]) - eth;
            gpx[i] += 0.62 * rx;
            gpy[i] += 0.62 * ry;
            gth[i] += 0.22 * rth;
            gpx[i - 1] -= 0.62 * rx;
            gpy[i - 1] -= 0.62 * ry;
            gth[i - 1] -= 0.22 * rth;
        }

        for (int i = 0; i < kPose; i += 3) {
            const int j = (i + 53) % kPose;
            const double ex = 0.45 * cos(0.017 * (double)(i + j));
            const double ey = 0.40 * sin(0.015 * (double)(i + j));
            const double rx = (px[j] - px[i]) - ex;
            const double ry = (py[j] - py[i]) - ey;
            gpx[i] -= 0.10 * rx;
            gpy[i] -= 0.10 * ry;
            gpx[j] += 0.10 * rx;
            gpy[j] += 0.10 * ry;
            gth[i] += 0.02 * (th[i] - th[j]);
            gth[j] -= 0.02 * (th[i] - th[j]);
        }

        for (int o = 0; o < kObs; ++o) {
            const int p = (o * 7 + 5) % kPose;
            const int l = (o * 11 + 3) % kLand;
            const double c = cos(th[p]);
            const double s = sin(th[p]);
            const double dx = lx[l] - px[p];
            const double dy = ly[l] - py[p];
            const double rx = c * dx + s * dy;
            const double ry = -s * dx + c * dy;

            const double tx = 0.35 * cos(0.019 * (double)o) + 0.04 * sin(0.07 * (double)(p + sweep));
            const double ty = 0.28 * sin(0.021 * (double)o) - 0.03 * cos(0.05 * (double)(l + sweep));
            const double ex = rx - tx;
            const double ey = ry - ty;
            const double w = 1.0 / (1.0 + 0.7 * (ex * ex + ey * ey));

            const double drx_dth = -s * dx + c * dy;
            const double dry_dth = -c * dx - s * dy;

            gpx[p] += w * (-ex * c + ey * s);
            gpy[p] += w * (-ex * s - ey * c);
            gth[p] += w * (ex * drx_dth + ey * dry_dth);
            glx[l] += w * (ex * c - ey * s);
            gly[l] += w * (ex * s + ey * c);
        }

        for (int i = 0; i < kPose; ++i) {
            gpx[i] += 0.008 * px[i];
            gpy[i] += 0.008 * py[i];
            gth[i] += 0.006 * th[i];
            px[i] -= 0.030 * gpx[i];
            py[i] -= 0.030 * gpy[i];
            th[i] -= 0.020 * gth[i];
        }
        for (int l = 0; l < kLand; ++l) {
            glx[l] += 0.006 * lx[l];
            gly[l] += 0.006 * ly[l];
            lx[l] -= 0.034 * glx[l];
            ly[l] -= 0.034 * gly[l];
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kPose; ++i) {
        acc += px[i] * px[i] + py[i] * py[i] + 0.25 * th[i] * th[i];
    }
    for (int l = 0; l < kLand; ++l) {
        acc += 0.35 * lx[l] * lx[l] + 0.35 * ly[l] * ly[l];
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#elif U2BENCH_EXTRA_KIND == 29

int main() {
    constexpr int kBody = 192;
    constexpr int kSteps = 96;
    constexpr int kIters = 6;

    static double x[kBody];
    static double y[kBody];
    static double vx[kBody];
    static double vy[kBody];
    static double r[kBody];
    static double invm[kBody];

    uint64_t seed = 113;
    for (int i = 0; i < kBody; ++i) {
        const int gx = i % 16;
        const int gy = i / 16;
        x[i] = -1.2 + 0.16 * (double)gx + 0.02 * u2bench_rand_sym(&seed);
        y[i] = -0.8 + 0.16 * (double)gy + 0.02 * u2bench_rand_sym(&seed);
        vx[i] = 0.04 * u2bench_rand_sym(&seed);
        vy[i] = 0.04 * u2bench_rand_sym(&seed);
        r[i] = 0.045 + 0.004 * (double)(i & 3);
        invm[i] = 1.0 / (1.0 + 0.25 * (double)(i & 7));
    }

    const uint64_t t0 = u2bench_now_ns();
    for (int step = 0; step < kSteps; ++step) {
        for (int i = 0; i < kBody; ++i) {
            vx[i] += 0.003 * sin(0.04 * (double)(step + i));
            vy[i] -= 0.010;
            x[i] += 0.06 * vx[i];
            y[i] += 0.06 * vy[i];
        }

        for (int iter = 0; iter < kIters; ++iter) {
            for (int i = 0; i < kBody; ++i) {
                const int j1 = (i + 1) % kBody;
                const int j2 = (i + 17) % kBody;
                const int j3 = (i * 7 + step * 3 + iter) % kBody;
                const int js[3] = {j1, j2, j3};
                for (int n = 0; n < 3; ++n) {
                    const int j = js[n];
                    if (j == i) {
                        continue;
                    }
                    double dx = x[j] - x[i];
                    double dy = y[j] - y[i];
                    double d2 = dx * dx + dy * dy + 1e-9;
                    double d = sqrt(d2);
                    double min_d = r[i] + r[j];
                    if (d < min_d) {
                        double nx = dx / d;
                        double ny = dy / d;
                        double overlap = min_d - d;
                        double rvx = vx[j] - vx[i];
                        double rvy = vy[j] - vy[i];
                        double reln = rvx * nx + rvy * ny;
                        double imp = 0.55 * overlap - 0.08 * reln;
                        if (imp < 0.0) {
                            imp = 0.0;
                        }
                        double w = invm[i] + invm[j] + 1e-9;
                        double corr = overlap / w;
                        x[i] -= corr * invm[i] * nx;
                        y[i] -= corr * invm[i] * ny;
                        x[j] += corr * invm[j] * nx;
                        y[j] += corr * invm[j] * ny;
                        vx[i] -= imp * invm[i] * nx;
                        vy[i] -= imp * invm[i] * ny;
                        vx[j] += imp * invm[j] * nx;
                        vy[j] += imp * invm[j] * ny;
                    }
                }

                const double box = 1.65;
                if (x[i] < -box) {
                    x[i] = -box;
                    vx[i] = fabs(vx[i]) * 0.72;
                } else if (x[i] > box) {
                    x[i] = box;
                    vx[i] = -fabs(vx[i]) * 0.72;
                }
                if (y[i] < -box) {
                    y[i] = -box;
                    vy[i] = fabs(vy[i]) * 0.68;
                } else if (y[i] > box) {
                    y[i] = box;
                    vy[i] = -fabs(vy[i]) * 0.68;
                }
            }
        }
    }
    const uint64_t t1 = u2bench_now_ns();

    double acc = 0.0;
    for (int i = 0; i < kBody; ++i) {
        acc += x[i] * x[i] + y[i] * y[i] + 0.4 * (vx[i] * vx[i] + vy[i] * vy[i]);
    }

    u2bench_sink_f64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

#else
#error "unsupported U2BENCH_EXTRA_KIND"
#endif
