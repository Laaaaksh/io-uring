# Stage 3 — Fixed files and registered buffers

**You'll be able to:** register files and buffers with a ring, use
`IOSQE_FIXED_FILE` and `*_fixed` ops to reference them by index, and
explain what that actually saves — with your own measurement, not a
claim you read.

**Time:** 1–2 hours.

**Build:** [`code/03-fixed-files-and-buffers`](../code/03-fixed-files-and-buffers).

## What this is for

Every plain `IORING_OP_READ` against a raw fd makes the kernel resolve that
fd through the process's file table (`fget`/`fput`) on every single call,
and pin whatever buffer you handed it for the duration of the I/O.
`io_uring_register_files()` and `io_uring_register_buffers()` tell the
kernel about a set of fds/buffers *once*, up front; operations that
reference them afterward by *index* instead of by fd/pointer skip both
costs. This is real, documented kernel behavior — and, as this stage's
sample shows, its effect size depends heavily on your workload.

## Read or watch, in this order

1. **[`io_uring_register(2)`](https://man7.org/linux/man-pages/man2/io_uring_register.2.html)**
   — `IORING_REGISTER_FILES` and `IORING_REGISTER_BUFFERS` specifically.
2. **[`io_uring_prep_read_fixed(3)`](https://man7.org/linux/man-pages/man3/io_uring_prep_read_fixed.3.html)**
   (part of liburing's own man tree) — note that the buffer pointer you pass
   still has to be a real address *within* the registered range; "fixed"
   means the kernel already knows about it, not that you can pass `NULL`.
   This repository's own sample hit exactly that mistake while being built
   — see [`resources/common-pitfalls.md`](../resources/common-pitfalls.md)
   if you want the story before hitting it yourself.

## Do

Build and run [`code/03-fixed-files-and-buffers`](../code/03-fixed-files-and-buffers)
a few times in a row. It times ~20,000 reads of the same small file two
ways — plain fd and buffer, then registered file and buffer — and reports
which was faster *in that run*. Read the README before trusting either
number: this repo's own testing saw the registered path faster by
single-digit percentages in some runs, and not faster at all in others, on
a workload this small and already page-cache-resident.

Then:

- Run it five times and note how much the result varies. What does that
  variance tell you about how much weight to put on any single benchmark
  run, on any machine?
- Read [`code/05-file-copy`](../code/05-file-copy)'s README once you get
  there — it measures a workload (many concurrent random reads) where a
  different form of "tell the kernel more up front" (keeping many ops in
  flight) shows an 11–23× effect, not a single-digit one. The contrast is
  the point: registration overhead is real, but whether it's *visible*
  depends entirely on how much per-operation cost your workload already
  has to hide it in.

## Checkpoint

- What specifically does `IOSQE_FIXED_FILE` change about how the kernel
  interprets the `fd` field of an SQE?
- Why does `io_uring_prep_read_fixed()` still need a real buffer pointer,
  if the buffer is already registered?
- This stage's sample sometimes measured the "fixed" path as *not* faster.
  Does that mean the mechanism doesn't work? What would make its effect
  more visible?

Next: [Stage 4 — networking](04-networking.md).
