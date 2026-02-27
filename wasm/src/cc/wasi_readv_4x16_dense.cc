#include "bench_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>

static inline bool write_all(int fd, const uint8_t* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        const ssize_t rc = write(fd, buf + off, len - off);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += (size_t)rc;
    }
    return true;
}

static inline bool readv_full64(int fd, struct iovec* iov, int iovcnt) {
    size_t remaining = 64;
    int idx = 0;
    int cnt = iovcnt;
    while (remaining > 0) {
        const ssize_t rc = readv(fd, iov + idx, cnt);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (rc == 0) {
            if (lseek(fd, 0, SEEK_SET) < 0) return false;
            continue;
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
    const char* path = "u2bench_readv.tmp";
    const int fdw = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fdw < 0) {
        printf("open(write) failed: %s\n", strerror(errno));
        return 1;
    }

    // Build a 4 KiB file with non-trivial contents.
    uint8_t filebuf[4096];
    uint32_t state = 1;
    for (size_t i = 0; i < sizeof(filebuf); ++i) {
        filebuf[i] = (uint8_t)u2bench_xorshift32(&state);
    }
    if (!write_all(fdw, filebuf, sizeof(filebuf))) {
        printf("write failed: %s\n", strerror(errno));
        close(fdw);
        return 1;
    }
    close(fdw);

    const int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        printf("open(read) failed: %s\n", strerror(errno));
        return 1;
    }

    alignas(16) uint8_t a[16];
    alignas(16) uint8_t b[16];
    alignas(16) uint8_t c[16];
    alignas(16) uint8_t d[16];

    struct iovec iov[4];
    iov[0].iov_base = a;
    iov[0].iov_len = sizeof(a);
    iov[1].iov_base = b;
    iov[1].iov_len = sizeof(b);
    iov[2].iov_base = c;
    iov[2].iov_len = sizeof(c);
    iov[3].iov_base = d;
    iov[3].iov_len = sizeof(d);

    constexpr int kOps = 200000; // 12.8 MiB total
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (int i = 0; i < kOps; ++i) {
        struct iovec cur[4] = {iov[0], iov[1], iov[2], iov[3]};
        if (!readv_full64(fd, cur, 4)) {
            printf("readv failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        acc += (uint64_t)a[i & 15] + (uint64_t)d[(i * 7) & 15];
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
