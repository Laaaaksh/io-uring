// The mental model, with no library between you and the kernel.
//
// Every other sample in this repo uses liburing, because that's what you'd
// actually use in real code. This one uses the three raw syscalls
// (io_uring_setup, io_uring_enter) and hand-rolled mmap calls instead, once,
// so the "submission queue / completion queue, both shared memory" idea
// isn't something you take on faith from a library that hides it.
//
// What this program does: open a file, submit one read of it through
// io_uring, wait for the read to complete, print what came back. One
// operation is a strange way to show off a batching API - the point here
// isn't throughput, it's making the rings themselves visible. Stage 2 uses
// liburing to do the same job in about a third of the code, once you know
// what the library is doing on your behalf.
//
// Verified against: Linux 6.12 (see repo root README for the exact
// environment). The io_uring_setup/enter ABI used here has been stable
// since Linux 5.1.
#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/io_uring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../common/common.h"

// glibc doesn't wrap these three syscalls - you're expected to go through
// liburing, which is precisely why this file calls syscall() directly.
static int io_uring_setup(unsigned entries, struct io_uring_params *p) {
    return (int)syscall(SYS_io_uring_setup, entries, p);
}

static int io_uring_enter(int fd, unsigned to_submit, unsigned min_complete,
                           unsigned flags) {
    return (int)syscall(SYS_io_uring_enter, fd, to_submit, min_complete,
                         flags, NULL, 0);
}

int main(void) {
    const char *path = "testfile.txt";
    const char *content = "Hello from io_uring - read via one submitted "
                           "SQE, no read() syscall in sight.\n";

    // Set up test data the same way every sample in this repo does: create
    // it fresh so the sample is self-contained and doesn't depend on
    // anything outside its own directory.
    int wfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK_ERRNO(wfd, "open (write)");
    CHECK_ERRNO(write(wfd, content, strlen(content)), "write");
    close(wfd);

    int fd = open(path, O_RDONLY);
    CHECK_ERRNO(fd, "open (read)");

    // --- Step 1: io_uring_setup ---
    // This asks the kernel for a ring with room for `entries` submissions
    // and returns a file descriptor representing the ring, plus a `params`
    // struct describing the memory layout the kernel actually allocated
    // (which can be larger than what you asked for - power-of-two rounded).
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    int ring_fd = io_uring_setup(8, &params);
    CHECK_ERRNO(ring_fd, "io_uring_setup");

    // This sample takes the one-mmap-covers-both-rings shortcut, which
    // every kernel in support today provides (the feature landed in 5.4).
    // Fail loudly instead of silently mmap'ing garbage on some kernel where
    // it's missing.
    if (!(params.features & IORING_FEAT_SINGLE_MMAP)) {
        fprintf(stderr,
                "kernel lacks IORING_FEAT_SINGLE_MMAP (pre-5.4) - this "
                "sample doesn't support the older two-mmap layout\n");
        return 1;
    }

    // --- Step 2: mmap the rings into this process's address space ---
    // The submission queue (SQ) and completion queue (CQ) are ring buffers
    // the kernel and this process both read and write, *without a syscall
    // on every operation* - that shared memory is the entire reason
    // io_uring can be faster than one-syscall-per-op interfaces. Kernels
    // with IORING_FEAT_SINGLE_MMAP (essentially all in-support kernels
    // today) let one mmap cover both the SQ ring and CQ ring; we still keep
    // the offsets separate because that's what the kernel handed back in
    // `params` and it's clearer to name them for what they are.
    size_t sq_ring_sz = params.sq_off.array + params.sq_entries * sizeof(unsigned);
    size_t cq_ring_sz = params.cq_off.cqes + params.cq_entries * sizeof(struct io_uring_cqe);
    size_t ring_sz = sq_ring_sz > cq_ring_sz ? sq_ring_sz : cq_ring_sz;

    void *sq_ring = mmap(NULL, ring_sz, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, ring_fd, IORING_OFF_SQ_RING);
    CHECK_ERRNO(sq_ring == MAP_FAILED ? -1 : 0, "mmap SQ ring");
    // With IORING_FEAT_SINGLE_MMAP the CQ ring lives in the same mapping.
    void *cq_ring = sq_ring;

    // The array of submission-queue *entries* (the actual SQEs - opcode,
    // fd, buffer pointer, length, offset) is a separate mapping from the
    // ring of *indices* into that array. This indirection is what lets you
    // submit SQEs out of the order they'll execute in.
    struct io_uring_sqe *sqes =
        mmap(NULL, params.sq_entries * sizeof(struct io_uring_sqe),
             PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd,
             IORING_OFF_SQES);
    CHECK_ERRNO(sqes == MAP_FAILED ? -1 : 0, "mmap SQEs");

    unsigned *sq_tail = (unsigned *)((char *)sq_ring + params.sq_off.tail);
    unsigned *sq_array = (unsigned *)((char *)sq_ring + params.sq_off.array);
    unsigned *cq_head = (unsigned *)((char *)cq_ring + params.cq_off.head);
    unsigned *cq_tail = (unsigned *)((char *)cq_ring + params.cq_off.tail);
    struct io_uring_cqe *cqes =
        (struct io_uring_cqe *)((char *)cq_ring + params.cq_off.cqes);

    // --- Step 3: build one SQE describing a read ---
    // This is the part liburing's io_uring_prep_read() does for you. Here
    // it's explicit: pick the next free slot, fill in the operation.
    char buf[256] = {0};
    unsigned index = *sq_tail & (params.sq_entries - 1); // ring is power-of-two sized
    struct io_uring_sqe *sqe = &sqes[index];
    memset(sqe, 0, sizeof(*sqe));
    sqe->opcode = IORING_OP_READ;
    sqe->fd = fd;
    sqe->addr = (unsigned long)buf;
    sqe->len = sizeof(buf) - 1;
    sqe->off = 0;
    sqe->user_data = 0x1234; // an opaque tag we chose, echoed back in the CQE

    // Publish the SQE: put its index in the SQ array, then advance the
    // tail. The kernel won't look at a slot until the tail says it's
    // there - this two-step (write the array slot, then bump the tail with
    // a memory barrier implied by the syscall boundary) is what makes the
    // ring safe to share without a lock.
    sq_array[index] = index;
    __atomic_store_n(sq_tail, *sq_tail + 1, __ATOMIC_RELEASE);

    // --- Step 4: io_uring_enter ---
    // This is the one syscall in this whole program. `to_submit=1` tells
    // the kernel one new SQE is ready; `min_complete=1,
    // IORING_ENTER_GETEVENTS` tells it to block until at least one
    // completion is ready, so we don't need a separate poll loop for this
    // minimal example.
    int submitted =
        io_uring_enter(ring_fd, 1, 1, IORING_ENTER_GETEVENTS);
    CHECK_ERRNO(submitted, "io_uring_enter");
    printf("io_uring_enter submitted %d SQE(s)\n", submitted);

    // --- Step 5: read the CQE ---
    // Same shared-memory pattern in reverse: the kernel wrote a completion,
    // advanced cq_tail, and we read up to that point.
    unsigned head = __atomic_load_n(cq_head, __ATOMIC_ACQUIRE);
    unsigned tail = __atomic_load_n(cq_tail, __ATOMIC_ACQUIRE);
    if (head == tail) {
        fprintf(stderr, "no completion available - unexpected\n");
        return 1;
    }
    struct io_uring_cqe *cqe = &cqes[head & (params.cq_entries - 1)];

    printf("completion: user_data=0x%llx res=%d (bytes read)\n",
           (unsigned long long)cqe->user_data, cqe->res);
    if (cqe->res < 0) {
        fprintf(stderr, "read failed: %s\n", strerror(-cqe->res));
        return 1;
    }
    buf[cqe->res] = '\0';
    printf("read %d bytes: %s", cqe->res, buf);

    // Tell the kernel we've consumed this completion, so it can reuse the
    // slot.
    __atomic_store_n(cq_head, head + 1, __ATOMIC_RELEASE);

    munmap(sqes, params.sq_entries * sizeof(struct io_uring_sqe));
    munmap(sq_ring, ring_sz);
    close(ring_fd);
    close(fd);
    unlink(path);
    return 0;
}
