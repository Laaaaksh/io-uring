# 02 — liburing basics

The same ideas as `01-raw-syscall`, through liburing's actual API:
`io_uring_queue_init`, `io_uring_get_sqe`, `io_uring_prep_read`,
`io_uring_submit`, `io_uring_wait_cqe`. Plus one new idea: `IOSQE_IO_LINK`,
and what happens to a linked chain when the first op fails.

Companion to [`curriculum/02-liburing-basics.md`](../../curriculum/02-liburing-basics.md).

## Build and run

```bash
make
./liburing_basics.out
```

## What was measured

This is a correctness/behavior demo. Real output from this repository's
own testing:

```
$ ./liburing_basics.out
--- 1. two independent reads, one submit, one wait per completion ---
submitted 2 SQEs in one syscall
  completion for read-a   res=31
  completion for read-b   res=31
buf_a="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
buf_b="BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

--- 2. IOSQE_IO_LINK: break the first op, watch the second get canceled ---
  link-1 (bad fd, will fail): res=-9 (Bad file descriptor)
  link-2 (would succeed alone): res=-125 (Operation canceled)
link-2 never touched the fd - it was canceled because link-1 failed, not skipped silently and not run out of order.
```

## What to look for

- Part 1 submits two reads at two different file offsets with **one**
  `io_uring_submit()` call — that's the batching win: one syscall
  regardless of how many SQEs were queued first. The completions are
  matched back to `buf_a`/`buf_b` by the `user_data` tag, not by assumed
  order.
- Part 2 is the important one: `link-1` is deliberately given an invalid fd
  (`-1`) so it fails with `-9` (`EBADF`). `link-2`, linked to it via
  `IOSQE_IO_LINK`, never runs at all — it completes with `-125`
  (`ECANCELED`). This is not a crash and not a silent skip; it's a real,
  distinct completion you have to handle. A production linked-chain
  implementation (like `code/05-file-copy`'s read-then-write pairs) has to
  treat `-ECANCELED` as its own case, not lump it in with a normal I/O
  error.
- Change `bad_fd` in `linked_chain()` to `good_fd` and re-run — both ops
  should now succeed, in the linked order.
