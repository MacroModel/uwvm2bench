#include "bench_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const char* path = "u2bench_filestat.tmp";
    const int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) {
        printf("open failed: %s\n", strerror(errno));
        return 1;
    }

    uint8_t buf[64];
    for (size_t i = 0; i < sizeof(buf); ++i) {
        buf[i] = (uint8_t)(i * 23u + 7u);
    }
    if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        printf("write failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    constexpr int kOps = 200000;
    uint64_t acc = 0;
    struct stat st;

    const uint64_t t0 = u2bench_now_ns();
    for (int i = 0; i < kOps; ++i) {
        if (fstat(fd, &st) != 0) {
            printf("fstat failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        acc += (uint64_t)st.st_size;
    }
    const uint64_t t1 = u2bench_now_ns();

    close(fd);
    {
        const int fd2 = open(path, O_WRONLY | O_TRUNC, 0644);
        if (fd2 >= 0) close(fd2);
    }

    u2bench_sink_u64(acc);
    u2bench_print_time_ns(t1 - t0);
    return 0;
}
