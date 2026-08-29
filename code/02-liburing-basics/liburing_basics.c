// liburing's actual API: queue_init, get_sqe, prep_*, submit, wait_cqe,
// cqe_seen. This is what code/01-raw-syscall was doing by hand, wrapped so
// you never touch a ring pointer or a memory barrier directly.
//
// Companion to curriculum/02-liburing-basics.md.
//
// This sample does two things:
//   1. Submits two independent reads in one batch and waits for both -
//      the basic prep/submit/wait loop you'll reuse in every later sample.
//   2. Links two SQEs with IOSQE_IO_LINK so the second only runs if the
//      first succeeds, then deliberately breaks the first one to show what
//      a linked failure actually looks like in the completion queue -
//      ECANCELED on the op that never ran, not a crash and not a silent
//      skip.
//
// Verified against: Linux 6.12, liburing 2.5 (see repo root README).
#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

#define QUEUE_DEPTH 8

static void write_test_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK_ERRNO(fd, "open (write)");
    CHECK_ERRNO(write(fd, content, strlen(content)), "write");
    close(fd);
}

static void batch_of_two(struct io_uring *ring, int fd) {
    printf("--- 1. two independent reads, one submit, one wait per completion ---\n");

    char buf_a[32] = {0}, buf_b[32] = {0};

    // io_uring_get_sqe hands you a slot in the submission queue. It
    // returns NULL if the queue is full (QUEUE_DEPTH entries here) - real
    // code checks that; this sample doesn't submit enough at once to hit
    // it, but check-for-NULL is the pattern to carry forward.
    struct io_uring_sqe *sqe_a = io_uring_get_sqe(ring);
    io_uring_prep_read(sqe_a, fd, buf_a, sizeof(buf_a) - 1, 0);   // offset 0
    io_uring_sqe_set_data(sqe_a, (void *)"read-a");

    struct io_uring_sqe *sqe_b = io_uring_get_sqe(ring);
    io_uring_prep_read(sqe_b, fd, buf_b, sizeof(buf_b) - 1, 32);  // offset 32
    io_uring_sqe_set_data(sqe_b, (void *)"read-b");

    // One submit call flushes both SQEs to the kernel in a single
    // io_uring_enter - this is the batching io_uring exists for. Two
    // read()s would have been two syscalls; this is one, regardless of how
    // many SQEs were queued first.
    int submitted = io_uring_submit(ring);
    CHECK_RC(submitted, "io_uring_submit");
    printf("submitted %d SQEs in one syscall\n", submitted);

    // Completions can arrive in either order - nothing here guarantees the
    // kernel finishes read-a before read-b. Wait for two, then look at
    // user_data (the tag we set above) to know which is which, not the
    // order they came back in.
    for (int i = 0; i < 2; i++) {
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(ring, &cqe), "io_uring_wait_cqe");
        const char *tag = (const char *)io_uring_cqe_get_data(cqe);
        printf("  completion for %-8s res=%d\n", tag, cqe->res);
        // Every CQE you take off the queue must be marked seen, or the
        // queue eventually fills and io_uring_get_sqe starts failing.
        io_uring_cqe_seen(ring, cqe);
    }
    printf("buf_a=\"%s\"\nbuf_b=\"%s\"\n\n", buf_a, buf_b);
}

static void linked_chain(struct io_uring *ring, int good_fd) {
    printf("--- 2. IOSQE_IO_LINK: break the first op, watch the second get canceled ---\n");

    char buf[32] = {0};
    int bad_fd = -1; // deliberately invalid, to make the first op fail

    struct io_uring_sqe *sqe1 = io_uring_get_sqe(ring);
    io_uring_prep_read(sqe1, bad_fd, buf, sizeof(buf) - 1, 0);
    io_uring_sqe_set_data(sqe1, (void *)"link-1 (bad fd, will fail)");
    // IOSQE_IO_LINK ties this SQE to the next one: the next SQE only
    // starts once this one completes, and if this one fails, the next is
    // completed with -ECANCELED instead of running at all. Without this
    // flag the two SQEs would run independently, and both order and
    // failure-of-one would say nothing about the other.
    io_uring_sqe_set_flags(sqe1, IOSQE_IO_LINK);

    struct io_uring_sqe *sqe2 = io_uring_get_sqe(ring);
    io_uring_prep_read(sqe2, good_fd, buf, sizeof(buf) - 1, 0);
    io_uring_sqe_set_data(sqe2, (void *)"link-2 (would succeed alone)");

    CHECK_RC(io_uring_submit(ring), "io_uring_submit");

    for (int i = 0; i < 2; i++) {
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(ring, &cqe), "io_uring_wait_cqe");
        const char *tag = (const char *)io_uring_cqe_get_data(cqe);
        if (cqe->res < 0) {
            printf("  %s: res=%d (%s)\n", tag, cqe->res, strerror(-cqe->res));
        } else {
            printf("  %s: res=%d\n", tag, cqe->res);
        }
        io_uring_cqe_seen(ring, cqe);
    }
    printf(
        "link-2 never touched the fd - it was canceled because link-1 "
        "failed, not skipped silently and not run out of order.\n");
}

int main(void) {
    const char *path = "testfile.txt";
    write_test_file(path,
                     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" // 32 bytes, offset 0
                     "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" // 32 bytes, offset 32
    );
    int fd = open(path, O_RDONLY);
    CHECK_ERRNO(fd, "open (read)");

    struct io_uring ring;
    // io_uring_queue_init wraps io_uring_setup + the mmaps from Stage 1
    // into one call. QUEUE_DEPTH is the number of in-flight SQEs the ring
    // can hold before io_uring_get_sqe starts returning NULL.
    CHECK_RC(io_uring_queue_init(QUEUE_DEPTH, &ring, 0),
             "io_uring_queue_init");

    batch_of_two(&ring, fd);
    linked_chain(&ring, fd);

    io_uring_queue_exit(&ring);
    close(fd);
    unlink(path);
    return 0;
}
