// A second measurement, because io_uring_cp.c's result (below) needs one:
// sequential copy of a fully page-cache-resident file barely exercises the
// thing io_uring is actually for. Random reads do - the kernel's
// readahead can't predict a random access pattern, so each read is a real
// independent operation, and whether N of them overlap or run one at a
// time genuinely matters.
//
// Same file, same total read volume, two strategies:
//   1. plain pread() at random offsets, one at a time.
//   2. QUEUE_DEPTH random preads kept in flight via io_uring at once,
//      refilling each slot as it completes.
//
// Companion to curriculum/05-storage.md - see that stage and this sample's
// README for what was actually measured in this repo's environment and
// why it can differ from yours.
#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../common/common.h"

#define QUEUE_DEPTH 32
#define READ_SIZE 4096
#define NUM_READS 20000

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static off_t random_offset(off_t file_size) {
    // Aligned to READ_SIZE so this also works unmodified if you switch the
    // fd to O_DIRECT to measure past the page cache - see the README.
    off_t max_start = file_size - READ_SIZE;
    off_t slot = max_start / READ_SIZE;
    return (off_t)(((double)rand() / RAND_MAX) * (double)slot) * READ_SIZE;
}

static double bench_plain(int fd, off_t file_size, char *buf) {
    srand(1);
    double t0 = now_ms();
    for (int i = 0; i < NUM_READS; i++) {
        off_t off = random_offset(file_size);
        ssize_t n = pread(fd, buf, READ_SIZE, off);
        CHECK_ERRNO(n, "pread");
    }
    return now_ms() - t0;
}

static double bench_uring(int fd, off_t file_size, char *buf) {
    struct io_uring ring;
    CHECK_RC(io_uring_queue_init(QUEUE_DEPTH, &ring, 0), "queue_init");

    srand(1); // same access pattern as bench_plain, for a fair comparison
    int issued = 0, completed = 0;

    double t0 = now_ms();
    for (int i = 0; i < QUEUE_DEPTH && issued < NUM_READS; i++, issued++) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        off_t off = random_offset(file_size);
        io_uring_prep_read(sqe, fd, buf + (size_t)i * READ_SIZE, READ_SIZE,
                            (__u64)off);
        io_uring_sqe_set_data(sqe, (void *)(intptr_t)i);
    }
    CHECK_RC(io_uring_submit(&ring), "initial submit");

    while (completed < NUM_READS) {
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(&ring, &cqe), "wait_cqe");
        int slot = (int)(intptr_t)io_uring_cqe_get_data(cqe);
        if (cqe->res < 0) {
            fprintf(stderr, "read failed: %s\n", strerror(-cqe->res));
            exit(1);
        }
        io_uring_cqe_seen(&ring, cqe);
        completed++;

        if (issued < NUM_READS) {
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
            off_t off = random_offset(file_size);
            io_uring_prep_read(sqe, fd, buf + (size_t)slot * READ_SIZE,
                                READ_SIZE, (__u64)off);
            io_uring_sqe_set_data(sqe, (void *)(intptr_t)slot);
            issued++;
            CHECK_RC(io_uring_submit(&ring), "resubmit");
        }
    }
    double elapsed = now_ms() - t0;
    io_uring_queue_exit(&ring);
    return elapsed;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        return 1;
    }
    int fd = open(argv[1], O_RDONLY);
    CHECK_ERRNO(fd, "open");
    struct stat st;
    CHECK_ERRNO(fstat(fd, &st), "fstat");

    char *buf = malloc(QUEUE_DEPTH * READ_SIZE);

    printf("%d random %d-byte reads from a %.0f MB file\n\n", NUM_READS,
           READ_SIZE, st.st_size / 1024.0 / 1024.0);

    double plain_ms = bench_plain(fd, st.st_size, buf);
    printf("plain pread(), one at a time:        %8.2f ms  (%.0f reads/sec)\n",
           plain_ms, NUM_READS / (plain_ms / 1000.0));

    double uring_ms = bench_uring(fd, st.st_size, buf);
    printf("io_uring, %2d in flight:              %8.2f ms  (%.0f reads/sec)\n",
           QUEUE_DEPTH, uring_ms, NUM_READS / (uring_ms / 1000.0));

    if (uring_ms < plain_ms) {
        printf("\nio_uring was %.1fx faster in this run.\n",
               plain_ms / uring_ms);
    } else {
        printf(
            "\nio_uring was NOT faster in this run - see the README for "
            "what that does and doesn't tell you.\n");
    }

    free(buf);
    close(fd);
    return 0;
}
