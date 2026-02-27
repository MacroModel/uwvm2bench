#include "bench_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

int main() {
    const char* path = "u2bench_open_missing.tmp";
    (void)unlink(path);

    constexpr uint32_t kOps = 200000u;
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        const int fd = open(path, O_RDONLY, 0);
        if (fd >= 0) {
            close(fd);
            printf("unexpected open success\n");
            return 1;
        }
        acc += (uint64_t)(uint32_t)errno;
    }
    const uint64_t t1 = u2bench_now_ns();

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

