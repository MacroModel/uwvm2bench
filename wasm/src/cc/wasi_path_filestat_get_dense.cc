#include "bench_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const char* path = "u2bench_filestat_path.tmp";
    {
        const int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd < 0) {
            printf("open failed: %s\n", strerror(errno));
            return 1;
        }
        const uint8_t b = 42;
        if (write(fd, &b, 1) != 1) {
            printf("write failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        close(fd);
    }

    struct stat st;
    uint64_t acc = 0;
    constexpr uint32_t kOps = 100000u;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        if (stat(path, &st) != 0) {
            printf("stat failed: %s\n", strerror(errno));
            return 1;
        }
        acc += (uint64_t)st.st_size + (uint64_t)(st.st_mode & 0xffu);
    }
    const uint64_t t1 = u2bench_now_ns();

    // Truncate (keep the path stable across runs).
    {
        const int fd = open(path, O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) close(fd);
    }

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}

