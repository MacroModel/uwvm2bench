#include "bench_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

static inline bool writev_full64(int fd, struct iovec* iov, int iovcnt) {
    size_t remaining = 64;
    int idx = 0;
    int cnt = iovcnt;
    while (remaining > 0) {
        const ssize_t rc = writev(fd, iov + idx, cnt);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (rc == 0) {
            return false;
        }
        size_t n = (size_t)rc;
        if (n > remaining) n = remaining;
        remaining -= n;
        while (n > 0 && cnt > 0) {
            const size_t len = iov[idx].iov_len;
            if (n < len) {
                iov[idx].iov_base = (uint8_t*)iov[idx].iov_base + n;
                iov[idx].iov_len = len - n;
                n = 0;
                break;
            }
            n -= len;
            idx += 1;
            cnt -= 1;
        }
        if (idx >= iovcnt) break;
    }
    return remaining == 0;
}

int main() {
    const char* path = "u2bench_writev.tmp";
    const int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        printf("open failed: %s\n", strerror(errno));
        return 1;
    }

    alignas(16) uint8_t a[16];
    alignas(16) uint8_t b[16];
    alignas(16) uint8_t c[16];
    alignas(16) uint8_t d[16];
    for (int i = 0; i < 16; ++i) {
        a[i] = (uint8_t)(i * 3 + 1);
        b[i] = (uint8_t)(i * 5 + 7);
        c[i] = (uint8_t)(i * 11 + 13);
        d[i] = (uint8_t)(i * 17 + 19);
    }

    struct iovec iov[4];
    iov[0].iov_base = a;
    iov[0].iov_len = sizeof(a);
    iov[1].iov_base = b;
    iov[1].iov_len = sizeof(b);
    iov[2].iov_base = c;
    iov[2].iov_len = sizeof(c);
    iov[3].iov_base = d;
    iov[3].iov_len = sizeof(d);

    constexpr int kOps = 100000; // 6.4 MiB total
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (int i = 0; i < kOps; ++i) {
        a[i & 15] ^= (uint8_t)i;
        struct iovec cur[4] = {iov[0], iov[1], iov[2], iov[3]};
        if (!writev_full64(fd, cur, 4)) {
            printf("writev failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        acc += (uint64_t)a[(i * 7) & 15];
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
