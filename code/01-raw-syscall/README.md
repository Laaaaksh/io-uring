# 01 — Raw syscall

The mental model with nothing between you and the kernel: `io_uring_setup`
and `io_uring_enter` called directly via `syscall()`, and the submission
and completion rings mmap'd by hand. No liburing.

Companion to [`curriculum/01-mental-model.md`](../../curriculum/01-mental-model.md).

## Build and run

```bash
make
./raw_read.out
```

## What it actually does

Creates a small test file, submits one `IORING_OP_READ` of it through a
hand-built ring, waits for the completion, and prints what came back.

## What was measured

This is a correctness demo, not a benchmark — one operation is a strange
way to show off a batching interface on purpose (see the comment at the
top of `raw_read.c`). Real output from this repository's own testing
(Linux 6.12, see the repo root README):

```
$ ./raw_read.out
io_uring_enter submitted 1 SQE(s)
completion: user_data=0x1234 res=78 (bytes read)
read 78 bytes: Hello from io_uring - read via one submitted SQE, no read() syscall in sight.
```

## What to look for

- `res=78` is the CQE's result field doing double duty as "bytes read" for
  a successful read op — the same field would hold a negative `-errno` on
  failure. Every later sample's error handling checks this same field the
  same way.
- `user_data=0x1234` is exactly what the code set before submitting —
  proof the completion queue is handing back an opaque tag you chose, not
  something the kernel invented. This is the whole mechanism every later
  sample uses to know which in-flight operation a completion belongs to.
- Try commenting out the `__atomic_store_n(sq_tail, ...)` line that
  publishes the SQE's index, without removing anything else. The kernel
  never sees an SQE you filled in but never announced via the tail — this
  breaks on purpose to make the "write the slot, then bump the tail"
  two-step protocol visible.
