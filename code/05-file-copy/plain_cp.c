// The comparison point for io_uring_cp.c: a plain, synchronous
// read()/write() copy loop, one chunk at a time, nothing in flight
// concurrently. Same chunk size, same file, so the only variable that
// changes between this and io_uring_cp.c is whether chunks overlap.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../common/common.h"

#define CHUNK_SIZE (256 * 1024)

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <src> <dst>\n", argv[0]);
        return 1;
    }
    int in_fd = open(argv[1], O_RDONLY);
    CHECK_ERRNO(in_fd, "open src");
    int out_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK_ERRNO(out_fd, "open dst");

    char *buf = malloc(CHUNK_SIZE);
    long total = 0;
    double t0 = now_ms();
    ssize_t n;
    while ((n = read(in_fd, buf, CHUNK_SIZE)) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(out_fd, buf + written, (size_t)(n - written));
            CHECK_ERRNO(w, "write");
            written += w;
        }
        total += n;
    }
    CHECK_ERRNO(n, "read");
    double t1 = now_ms();

    printf("copied %ld bytes in %.2f ms (%.1f MB/s)\n", total, t1 - t0,
           (total / 1024.0 / 1024.0) / ((t1 - t0) / 1000.0));

    free(buf);
    close(in_fd);
    close(out_fd);
    return 0;
}
