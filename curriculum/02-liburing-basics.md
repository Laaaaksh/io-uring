# Stage 2 — liburing basics

**You'll be able to:** use liburing's actual API (`queue_init`, `get_sqe`,
`prep_*`, `submit`, `wait_cqe`) fluently, and explain exactly what happens
when a linked chain of operations hits a failure partway through.

**Time:** 2–3 hours.

**Build:** [`code/02-liburing-basics`](../code/02-liburing-basics).

## What changes from Stage 1

Nothing conceptually — [Stage 1](01-mental-model.md)'s raw syscalls and this
stage's liburing calls do the same work. What changes is that liburing
handles the ring setup, the mmaps, and the shared-memory bookkeeping for
you, so the code you write is about the *operations* (read this, write
that, chain these together) instead of the plumbing underneath them.

## Read or watch, in this order

1. **[`axboe/liburing`](https://github.com/axboe/liburing)'s `examples/`
   directory** — browse it on GitHub. These are real, working programs from
   the library's own maintainer; skimming a few builds intuition for the
   prep/submit/wait shape faster than prose does.
2. **[`io_uring_enter(2)`](https://man7.org/linux/man-pages/man2/io_uring_enter.2.html)**
   — specifically the section on `IOSQE_IO_LINK`, which
   [`code/02-liburing-basics`](../code/02-liburing-basics) uses to chain two
   operations together.

## Do

Build and run [`code/02-liburing-basics`](../code/02-liburing-basics). It
does two things, in order — read the code alongside the output:

1. **Two independent reads, one `io_uring_submit()` call.** This is the
   batching io_uring exists for: two operations queued, one syscall to
   flush both to the kernel, two completions waited for by tag
   (`user_data`) rather than by assuming an order.
2. **A linked chain that deliberately fails.** `IOSQE_IO_LINK` ties one SQE
   to the next: the second only runs if the first succeeds. The sample
   makes the first op fail on purpose (a bad fd) and shows the second one's
   completion — it isn't skipped silently and it isn't run anyway, it comes
   back with `-ECANCELED`. This is the mechanism
   [`code/05-file-copy`](../code/05-file-copy)'s read-then-write chunk
   copies rely on later.

Then, on your own:

- Change the bad fd in the linked-chain example to a *valid* fd and confirm
  both ops now succeed and run in the linked order.
- Add a third linked SQE after the second one and confirm that if the
  *second* op fails, the third one also comes back `-ECANCELED` — a link
  chain's cancellation propagates through the whole remaining chain, not
  just to the next op.

## Checkpoint

- What's the actual difference between calling `io_uring_get_sqe()` twice
  then `io_uring_submit()` once, versus calling `get_sqe` +
  `io_uring_submit()` twice?
- If two SQEs are submitted *without* `IOSQE_IO_LINK`, and the first one
  fails, what happens to the second? How is that different from the linked
  case this sample demonstrates?
- Every CQE this sample reads gets `io_uring_cqe_seen()` called on it.
  What would eventually happen if you stopped calling that?

Next: [Stage 3 — fixed files and buffers](03-fixed-files-and-buffers.md).
