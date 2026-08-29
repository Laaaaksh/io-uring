// Registered (fixed) files and buffers: telling the kernel about your fds
// and buffers once, up front, instead of on every single operation.
//
// Companion to curriculum/03-fixed-files-and-buffers.md.
//
// Every plain IORING_OP_READ against a raw fd makes the kernel resolve that
// fd through the process's file table (fget/fput) on every call, and pin
// whatever buffer you handed it for the duration of the I/O. Registering a
// file (io_uring_register_files) and a buffer (io_uring_register_buffers)
// up front skips both costs on every subsequent operation that references
// them by their registered *index* instead of by raw fd/pointer.
//
// This sample runs the same read a few thousand times two ways - plain fd,
// then fixed file + fixed buffer - and times both. Read the README before
// trusting the numbers: this is a real measurement from a single run in a
// shared, virtualized environment, not a controlled benchmark, and it
// isolates one variable badly (see below).
//
// Verified against: Linux 6.12, liburing 2.5 (see repo root README).
#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../common/common.h"

#define ITERATIONS 20000
#define BUF_SIZE 64

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void run_plain(struct io_uring *ring, int fd, int n) {
    char buf[BUF_SIZE];
    for (int i = 0; i < n; i++) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
        io_uring_prep_read(sqe, fd, buf, sizeof(buf), 0);
        CHECK_RC(io_uring_submit(ring), "submit (plain)");
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(ring, &cqe), "wait_cqe (plain)");
        if (cqe->res < 0) {
            fprintf(stderr, "plain read failed: %s\n", strerror(-cqe->res));
            exit(1);
        }
        io_uring_cqe_seen(ring, cqe);
    }
}

static void run_fixed(struct io_uring *ring, void *buf, int n) {
    for (int i = 0; i < n; i++) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
        // fd index 0 into the registered-files table (set up in main),
        // buffer index 0 into the registered-buffers table. The fd is a
        // slot the kernel already knows about, not a real fd it has to
        // resolve - but the buffer pointer still has to be a real address
        // *within* the range registered for that index; the kernel checks
        // that, it doesn't ignore addr just because the buffer is fixed.
        io_uring_prep_read_fixed(sqe, 0, buf, BUF_SIZE, 0, 0);
        sqe->flags |= IOSQE_FIXED_FILE;
        CHECK_RC(io_uring_submit(ring), "submit (fixed)");
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(ring, &cqe), "wait_cqe (fixed)");
        if (cqe->res < 0) {
            fprintf(stderr, "fixed read failed: %s\n", strerror(-cqe->res));
            exit(1);
        }
        io_uring_cqe_seen(ring, cqe);
    }
}

int main(void) {
    const char *path = "testfile.txt";
    int wfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK_ERRNO(wfd, "open (write)");
    char content[BUF_SIZE];
    memset(content, 'x', sizeof(content));
    CHECK_ERRNO(write(wfd, content, sizeof(content)), "write");
    close(wfd);

    int fd = open(path, O_RDONLY);
    CHECK_ERRNO(fd, "open (read)");

    struct io_uring ring;
    CHECK_RC(io_uring_queue_init(8, &ring, 0), "io_uring_queue_init");

    // Register the file: from here on, index 0 in ring's internal table
    // refers to this fd. IOSQE_FIXED_FILE on an SQE tells the kernel "the
    // fd field is actually a registered-file index."
    int files[] = {fd};
    CHECK_RC(io_uring_register_files(&ring, files, 1),
             "io_uring_register_files");

    // Register a buffer the same way: one buffer, pinned once, referenced
    // by index (the last two args to io_uring_prep_read_fixed) instead of
    // by pointer on every call.
    static char fixed_buf[BUF_SIZE] __attribute__((aligned(4096)));
    struct iovec iov = {.iov_base = fixed_buf, .iov_len = sizeof(fixed_buf)};
    CHECK_RC(io_uring_register_buffers(&ring, &iov, 1),
             "io_uring_register_buffers");

    printf("warming up (%d iterations, not timed)...\n", ITERATIONS / 10);
    run_plain(&ring, fd, ITERATIONS / 10);
    run_fixed(&ring, fixed_buf, ITERATIONS / 10);

    printf("timing %d iterations each...\n", ITERATIONS);
    double t0 = now_ms();
    run_plain(&ring, fd, ITERATIONS);
    double t1 = now_ms();
    run_fixed(&ring, fixed_buf, ITERATIONS);
    double t2 = now_ms();

    double plain_ms = t1 - t0, fixed_ms = t2 - t1;
    printf("\nplain fd + plain buffer: %8.2f ms  (%.3f us/op)\n", plain_ms,
           plain_ms * 1000.0 / ITERATIONS);
    printf("fixed file + fixed buf:  %8.2f ms  (%.3f us/op)\n", fixed_ms,
           fixed_ms * 1000.0 / ITERATIONS);
    if (fixed_ms < plain_ms) {
        printf("fixed path was %.1f%% faster in this run.\n",
               (plain_ms - fixed_ms) * 100.0 / plain_ms);
    } else {
        printf(
            "fixed path was NOT faster in this run - see the README for "
            "why that can happen and doesn't mean the mechanism is "
            "pointless.\n");
    }

    io_uring_unregister_buffers(&ring);
    io_uring_unregister_files(&ring);
    io_uring_queue_exit(&ring);
    close(fd);
    unlink(path);
    return 0;
}
