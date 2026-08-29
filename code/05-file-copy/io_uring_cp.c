// A file-copy tool with N chunk-copies kept in flight at once, each one a
// read and write linked together so the write can't run before its read
// finishes and land wrong data.
//
// Companion to curriculum/05-storage.md.
//
// The idea this sample exists to show: io_uring's throughput advantage on
// storage workloads comes from having many operations in flight
// simultaneously, not from any one operation being faster than read()/
// write(). One chunk copied via io_uring is not faster than
// read()+write() - it's the same two syscalls' worth of *work*, just
// issued without blocking. The win shows up when QUEUE_DEPTH chunks are
// all in flight at once, overlapping their I/O wait time instead of doing
// it one chunk at a time.
//
// Each chunk-copy is two SQEs, linked with IOSQE_IO_LINK: read chunk i,
// then write chunk i to the same offset in the output - linked so the
// write is only attempted if the read actually succeeded, exactly like
// code/02-liburing-basics's linked-chain example, just used for something
// real this time. Up to QUEUE_DEPTH chunk-copies (so 2×QUEUE_DEPTH linked
// SQE pairs) are in flight at once; as each pair completes, the next
// unread chunk is queued into that freed slot.
//
// Verified against: Linux 6.12, liburing 2.5 (see repo root README). This
// sample uses buffered I/O (no O_DIRECT) so it runs the same on any
// filesystem, including the overlay/tmpfs-backed ones common in
// containers - curriculum/05-storage.md covers what O_DIRECT changes and
// why this sample doesn't use it.
#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../common/common.h"

#define CHUNK_SIZE (256 * 1024)
#define QUEUE_DEPTH 8

enum slot_state { SLOT_FREE, SLOT_READING, SLOT_WRITING };

struct slot {
    enum slot_state state;
    char *buf;
    off_t offset;
    size_t len;
};

// user_data encodes both the slot index and which half of the linked pair
// this completion is for, packed into the pointer value - simpler than a
// heap allocation per SQE for a sample this size, and it's a legitimate
// pattern: user_data is just a u64, it doesn't have to be a pointer.
static void *tag(int slot_idx, int is_write) {
    return (void *)(intptr_t)((slot_idx << 1) | is_write);
}
static int tag_slot(void *t) { return (int)((intptr_t)t >> 1); }
static int tag_is_write(void *t) { return (int)((intptr_t)t & 1); }

static void queue_chunk(struct io_uring *ring, struct slot *slots, int idx,
                         int in_fd, int out_fd, off_t offset, size_t len) {
    struct slot *s = &slots[idx];
    s->state = SLOT_READING;
    s->offset = offset;
    s->len = len;

    struct io_uring_sqe *read_sqe = io_uring_get_sqe(ring);
    io_uring_prep_read(read_sqe, in_fd, s->buf, (unsigned)len, (__u64)offset);
    io_uring_sqe_set_flags(read_sqe, IOSQE_IO_LINK);
    io_uring_sqe_set_data(read_sqe, tag(idx, 0));

    struct io_uring_sqe *write_sqe = io_uring_get_sqe(ring);
    io_uring_prep_write(write_sqe, out_fd, s->buf, (unsigned)len, (__u64)offset);
    io_uring_sqe_set_data(write_sqe, tag(idx, 1));
    (void)out_fd;
}

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
    const char *src_path = argv[1], *dst_path = argv[2];

    int in_fd = open(src_path, O_RDONLY);
    CHECK_ERRNO(in_fd, "open src");
    struct stat st;
    CHECK_ERRNO(fstat(in_fd, &st), "fstat");
    off_t file_size = st.st_size;

    int out_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK_ERRNO(out_fd, "open dst");
    CHECK_ERRNO(ftruncate(out_fd, file_size), "ftruncate dst");

    struct io_uring ring;
    // Two SQEs per in-flight chunk (read + linked write), so the ring
    // needs 2*QUEUE_DEPTH entries to hold a full window without stalling
    // on a full submission queue.
    CHECK_RC(io_uring_queue_init(2 * QUEUE_DEPTH, &ring, 0),
             "io_uring_queue_init");

    struct slot slots[QUEUE_DEPTH];
    for (int i = 0; i < QUEUE_DEPTH; i++) {
        slots[i].buf = malloc(CHUNK_SIZE);
        slots[i].state = SLOT_FREE;
    }

    double t0 = now_ms();
    off_t next_offset = 0;
    long chunks_in_flight = 0;
    long bytes_written = 0;

    // Prime the pipeline: queue up to QUEUE_DEPTH chunks before waiting for
    // anything, so all of them can be in flight together.
    for (int i = 0; i < QUEUE_DEPTH && next_offset < file_size; i++) {
        size_t len = (size_t)((file_size - next_offset) < CHUNK_SIZE
                                   ? (file_size - next_offset)
                                   : CHUNK_SIZE);
        queue_chunk(&ring, slots, i, in_fd, out_fd, next_offset, len);
        next_offset += (off_t)len;
        chunks_in_flight++;
    }
    CHECK_RC(io_uring_submit(&ring), "initial submit");

    while (chunks_in_flight > 0) {
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(&ring, &cqe), "wait_cqe");
        void *t = io_uring_cqe_get_data(cqe);
        int idx = tag_slot(t), is_write = tag_is_write(t);
        int res = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        if (res < 0) {
            fprintf(stderr, "chunk %d (%s) failed: %s\n", idx,
                    is_write ? "write" : "read", strerror(-res));
            exit(1);
        }

        if (is_write) {
            bytes_written += res;
            chunks_in_flight--;
            slots[idx].state = SLOT_FREE;
            // This slot just freed up - if there's more file left to copy,
            // queue the next chunk into it so QUEUE_DEPTH stays fully
            // utilized instead of draining down to zero at the end of
            // every window.
            if (next_offset < file_size) {
                size_t len = (size_t)((file_size - next_offset) < CHUNK_SIZE
                                           ? (file_size - next_offset)
                                           : CHUNK_SIZE);
                queue_chunk(&ring, slots, idx, in_fd, out_fd, next_offset, len);
                next_offset += (off_t)len;
                chunks_in_flight++;
                CHECK_RC(io_uring_submit(&ring), "resubmit");
            }
        }
        // A short read (res < requested len) on a regular file mid-copy
        // would be a real bug to handle in production code; this sample
        // doesn't handle it because the file sizes it's tested with don't
        // trigger it - see curriculum/05-storage.md for why short reads
        // happen and how to handle them.
    }

    double t1 = now_ms();
    printf("copied %ld bytes in %.2f ms (%.1f MB/s)\n", bytes_written,
           t1 - t0, (bytes_written / 1024.0 / 1024.0) / ((t1 - t0) / 1000.0));

    for (int i = 0; i < QUEUE_DEPTH; i++)
        free(slots[i].buf);
    io_uring_queue_exit(&ring);
    close(in_fd);
    close(out_fd);
    return 0;
}
