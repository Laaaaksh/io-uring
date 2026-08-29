// SQPOLL: a dedicated kernel thread polls the submission queue, so
// submitting work can skip the io_uring_enter syscall entirely, as long as
// that thread hasn't gone idle. This sample makes that concrete by running
// the same many-small-reads workload with a plain ring and an SQPOLL ring
// and timing both, using identical liburing calls in both cases - the only
// difference is one flag at ring-creation time.
//
// Companion to curriculum/06-sqpoll.md.
//
// liburing's io_uring_submit() already does the right thing for SQPOLL:
// it checks the ring's IORING_SQ_NEED_WAKEUP flag and only makes the
// io_uring_enter syscall if the poll thread has actually gone to sleep. So
// this sample doesn't need any SQPOLL-specific submission code.
//
// Both runs use a sliding window (at most WINDOW ops in flight at once,
// same shape as code/05-file-copy's samples), not "submit all 5000, then
// wait for all 5000." That's not just style - a ring can only hold
// `entries` SQEs that the kernel hasn't consumed yet, and with SQPOLL the
// kernel thread consumes them on its own schedule, not synchronously
// inside your submit call. Submit faster than it can keep up (which an
// earlier, unwindowed version of this sample did) and io_uring_get_sqe()
// starts returning NULL - real backpressure that real code has to handle,
// not a bug to work around once and forget.
//
// Verified against: Linux 6.12, liburing 2.5 (see repo root README).
#include <errno.h>
#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../common/common.h"

#define NUM_OPS 20000
#define WINDOW 64
#define BUF_SIZE 64

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// All WINDOW in-flight reads target the same buffer and offset - this
// benchmark only cares about completion timing, not the data, so the
// harmless race on `buf` (every op reads the same unchanging bytes) isn't
// worth a buffer per slot here. Don't copy this shortcut into code where
// the result matters - code/05-file-copy's samples show the pattern with
// one buffer per in-flight slot, which real code needs.

// io_uring_get_sqe() returns NULL when the ring has WINDOW SQEs the kernel
// hasn't dequeued yet - real backpressure, not an error. Under SQPOLL this
// is more likely to actually happen than with a plain ring: the polling
// thread dequeues on its own schedule instead of synchronously inside
// your submit call, and a fast userspace loop can briefly outrun it. The
// fix is exactly what you'd do if a queue anywhere else were full: wait
// for room. io_uring_submit() flushes anything genuinely pending (a
// harmless no-op if there's nothing to flush) and give the poll thread a
// moment to catch up.
static struct io_uring_sqe *get_sqe_wait(struct io_uring *ring) {
    struct io_uring_sqe *sqe;
    while ((sqe = io_uring_get_sqe(ring)) == NULL) {
        io_uring_submit(ring);
        usleep(100);
    }
    return sqe;
}

static double run_workload(struct io_uring *ring, int fd, char *buf) {
    double t0 = now_ms();
    int issued = 0, completed = 0;

    for (int i = 0; i < WINDOW && issued < NUM_OPS; i++, issued++) {
        struct io_uring_sqe *sqe = get_sqe_wait(ring);
        io_uring_prep_read(sqe, fd, buf, BUF_SIZE, 0);
        CHECK_RC(io_uring_submit(ring), "io_uring_submit (prime)");
    }

    while (completed < NUM_OPS) {
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(ring, &cqe), "io_uring_wait_cqe");
        if (cqe->res < 0) {
            fprintf(stderr, "read failed: %s\n", strerror(-cqe->res));
            exit(1);
        }
        io_uring_cqe_seen(ring, cqe);
        completed++;

        if (issued < NUM_OPS) {
            struct io_uring_sqe *sqe = get_sqe_wait(ring);
            io_uring_prep_read(sqe, fd, buf, BUF_SIZE, 0);
            issued++;
            CHECK_RC(io_uring_submit(ring), "io_uring_submit (refill)");
        }
    }

    return now_ms() - t0;
}

int main(void) {
    const char *path = "testfile.txt";
    int wfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK_ERRNO(wfd, "open (write)");
    char content[BUF_SIZE];
    memset(content, 'x', sizeof(content));
    CHECK_ERRNO(write(wfd, content, sizeof(content)), "write");
    close(wfd);

    char buf[BUF_SIZE];

    // --- Plain ring ---
    int fd1 = open(path, O_RDONLY);
    CHECK_ERRNO(fd1, "open (read, plain)");
    struct io_uring plain_ring;
    CHECK_RC(io_uring_queue_init(WINDOW, &plain_ring, 0), "queue_init (plain)");
    double plain_ms = run_workload(&plain_ring, fd1, buf);
    io_uring_queue_exit(&plain_ring);
    close(fd1);

    // --- SQPOLL ring ---
    // sq_thread_idle is how long (ms) the kernel thread keeps polling with
    // no work before going to sleep. Set generously here so it stays awake
    // for this whole benchmark - a real, latency-sensitive SQPOLL user
    // would tune this against actual request-rate gaps, not benchmark
    // convenience. See curriculum/06-sqpoll.md for that trade-off,
    // including the CPU-pinning cost SQPOLL always has: this thread spins,
    // burning a full core, for as long as it stays awake.
    int fd2 = open(path, O_RDONLY);
    CHECK_ERRNO(fd2, "open (read, sqpoll)");
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 30000; // 30s - long enough to cover this run
    struct io_uring sqpoll_ring;
    int rc = io_uring_queue_init_params(WINDOW, &sqpoll_ring, &params);
    if (rc == -EPERM) {
        fprintf(stderr,
                "IORING_SETUP_SQPOLL rejected with EPERM - some container "
                "or sandboxing setups restrict it even where plain "
                "io_uring is allowed; see curriculum/07-security.md and "
                "this sample's README.\n");
        return 1;
    }
    CHECK_RC(rc, "queue_init_params (sqpoll)");
    double sqpoll_ms = run_workload(&sqpoll_ring, fd2, buf);
    io_uring_queue_exit(&sqpoll_ring);
    close(fd2);

    printf("%d single-read ops, %d in flight at a time\n\n", NUM_OPS, WINDOW);
    printf("plain ring:  %8.2f ms  (%.0f ops/sec)\n", plain_ms,
           NUM_OPS / (plain_ms / 1000.0));
    printf("SQPOLL ring: %8.2f ms  (%.0f ops/sec)\n", sqpoll_ms,
           NUM_OPS / (sqpoll_ms / 1000.0));
    if (sqpoll_ms < plain_ms) {
        printf("\nSQPOLL was %.1fx faster in this run.\n", plain_ms / sqpoll_ms);
    } else {
        printf(
            "\nSQPOLL was NOT faster in this run - see the README for "
            "what that does and doesn't tell you.\n");
    }

    unlink(path);
    return 0;
}
