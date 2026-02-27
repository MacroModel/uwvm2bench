#include "bench_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
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

static inline bool pwrite_full(int fd, const uint8_t* buf, size_t len, off_t off) {
    size_t done = 0;
    while (done < len) {
        const ssize_t rc = pwrite(fd, buf + done, len - done, off + (off_t)done);
        if (rc < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (rc == 0) return false;
        done += (size_t)rc;
    }
    return true;
}

int main() {
    const char* path = "u2bench_pwrite.tmp";
    constexpr size_t kFileSize = 64u * 1024u;

    const int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0) {
        printf("open failed: %s\n", strerror(errno));
        return 1;
    }

    // Pre-size file to keep the timed loop steady.
    {
        uint8_t buf[4096];
        for (size_t i = 0; i < sizeof(buf); ++i) buf[i] = (uint8_t)i;
        size_t written = 0;
        while (written < kFileSize) {
            const size_t n = (kFileSize - written) < sizeof(buf) ? (kFileSize - written) : sizeof(buf);
            if (!write_all(fd, buf, n)) {
                printf("write failed: %s\n", strerror(errno));
                close(fd);
                return 1;
            }
            written += n;
        }
    }

    constexpr uint32_t kOps = 50000u;
    uint32_t state = 1;
    uint8_t buf[64];
    for (int i = 0; i < 64; ++i) buf[i] = (uint8_t)(i * 7 + 3);
    uint64_t acc = 0;

    const uint64_t t0 = u2bench_now_ns();
    for (uint32_t i = 0; i < kOps; ++i) {
        const uint32_t r = u2bench_xorshift32(&state);
        const off_t off = (off_t)((size_t)r & (kFileSize - 64u));
        buf[i & 63] ^= (uint8_t)i;
        if (!pwrite_full(fd, buf, sizeof(buf), off)) {
            printf("pwrite failed: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        acc += (uint64_t)buf[(i * 3u) & 63];
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

