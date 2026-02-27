#include "bench_common.h"

#include <stdint.h>
#include <wasi/api.h>

int main() {
    __wasi_subscription_t sub = {};
    sub.userdata = 0x1234u;
    sub.u.tag = __WASI_EVENTTYPE_CLOCK;
    sub.u.u.clock.id = __WASI_CLOCKID_MONOTONIC;
    sub.u.u.clock.timeout = 0;   // relative, immediate
    sub.u.u.clock.precision = 0; // no coalescing
    sub.u.u.clock.flags = 0;

    __wasi_event_t ev = {};
    __wasi_size_t nevents = 0;

    // Warm up + verify.
    const __wasi_errno_t e0 = __wasi_poll_oneoff(&sub, &ev, 1, &nevents);
    if (e0 != __WASI_ERRNO_SUCCESS || nevents != 1 || ev.error != __WASI_ERRNO_SUCCESS || ev.type != __WASI_EVENTTYPE_CLOCK) {
        printf("poll_oneoff(clock) failed: e=%u nevents=%u ev.error=%u ev.type=%u\n", (unsigned)e0, (unsigned)nevents, (unsigned)ev.error,
               (unsigned)ev.type);
        return 1;
    }

    constexpr uint32_t kOps = 200000u;
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        nevents = 0;
        const __wasi_errno_t e = __wasi_poll_oneoff(&sub, &ev, 1, &nevents);
        acc += (uint64_t)e + (uint64_t)nevents + (uint64_t)ev.type + (uint64_t)ev.error;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

