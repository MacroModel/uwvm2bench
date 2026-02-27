#include "bench_common.h"

#include <stdint.h>
#include <stdlib.h>
#include <wasi/api.h>

static inline __wasi_fd_t find_preopen_fd() {
    __wasi_prestat_t ps;
    for (__wasi_fd_t fd = 3; fd < 64; ++fd) {
        if (__wasi_fd_prestat_get(fd, &ps) == __WASI_ERRNO_SUCCESS) {
            return fd;
        }
    }
    return (__wasi_fd_t)UINT32_MAX;
}

int main() {
    const __wasi_fd_t fd = find_preopen_fd();
    if (fd == (__wasi_fd_t)UINT32_MAX) {
        printf("no preopened directory fd found\n");
        return 1;
    }

    __wasi_prestat_t ps = {};
    const __wasi_errno_t e0 = __wasi_fd_prestat_get(fd, &ps);
    if (e0 != __WASI_ERRNO_SUCCESS || ps.tag != __WASI_PREOPENTYPE_DIR) {
        printf("fd_prestat_get failed: fd=%u e=%u tag=%u\n", (unsigned)fd, (unsigned)e0, (unsigned)ps.tag);
        return 1;
    }

    __wasi_size_t name_len = ps.u.dir.pr_name_len;
    if (name_len == 0) {
        name_len = 1;
    }
    uint8_t* buf = (uint8_t*)malloc((size_t)name_len);
    if (!buf) {
        printf("malloc failed\n");
        return 1;
    }

    // Warm up + verify.
    const __wasi_errno_t e1 = __wasi_fd_prestat_dir_name(fd, buf, name_len);
    if (e1 != __WASI_ERRNO_SUCCESS) {
        printf("fd_prestat_dir_name failed: fd=%u e=%u len=%u\n", (unsigned)fd, (unsigned)e1, (unsigned)name_len);
        free(buf);
        return 1;
    }

    constexpr uint32_t kOps = 200000u;
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        __wasi_prestat_t cur = {};
        const __wasi_errno_t a = __wasi_fd_prestat_get(fd, &cur);
        const __wasi_errno_t b = __wasi_fd_prestat_dir_name(fd, buf, name_len);
        acc += (uint64_t)a + (uint64_t)b + (uint64_t)buf[i % name_len] + (uint64_t)cur.u.dir.pr_name_len;
    }
    const uint64_t t1 = u2bench_now_ns();

    free(buf);
    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

