# Stage 5 — Storage

**You'll be able to:** pipeline storage operations with a configurable
in-flight depth, and — this is the real point of the stage — correctly
predict *when* that helps and when it doesn't, backed by two contradictory
measurements from this repository's own testing.

**Time:** 3–4 hours.

**Build:** [`code/05-file-copy`](../code/05-file-copy).

## The claim this stage tests instead of asserting

Most io_uring material asserts that batching operations and keeping many
in flight makes storage I/O faster. That's true, but it's true for a
specific reason — overlapping I/O *wait* time across operations instead of
paying it serially — and that reason doesn't apply equally to every
workload. This stage's sample runs two different storage benchmarks and
reports what actually happened in this repository's own environment, not
what the common narrative predicts:

1. **Sequential file copy** (`io_uring_cp.c` vs. `plain_cp.c`): both copy
   the same 64MB file, one with 8 chunk-copies (linked read+write pairs)
   kept in flight via io_uring, one with a plain synchronous
   `read()`/`write()` loop. **The plain loop was not slower** — on a
   sequential access pattern to an already page-cache-resident file, the
   kernel's own readahead already pipelines the reads "for free," so there
   was little I/O wait left for io_uring's concurrency to overlap.
2. **Random reads** (`random_read_bench.c`): the same file, 20,000 random
   4KB reads, one at a time vs. 32 kept in flight via io_uring. **io_uring
   was 11–23× faster** across repeated runs in this repo's own testing —
   random access defeats the kernel's readahead prediction entirely, so
   each read is a genuinely independent operation, and whether 32 of them
   overlap or run one at a time is the whole game.

Both numbers are real measurements from this repository's own environment
(see the sample's README for the exact figures and how to reproduce them),
not asserted claims — see the house standard in this repo's `AGENTS.md` for
why that distinction matters.

## Read or watch, in this order

1. **[`io_uring_prep_read(3)`](https://man7.org/linux/man-pages/man3/io_uring_prep_read.3.html)**
   and **[`io_uring_prep_write(3)`](https://man7.org/linux/man-pages/man3/io_uring_prep_write.3.html)**
   — the two ops this stage's samples chain together.
2. Re-read [`code/02-liburing-basics`](../code/02-liburing-basics)'s linked-SQE
   section if it's not fresh — `io_uring_cp.c` uses exactly that pattern
   (`IOSQE_IO_LINK` between a read and its corresponding write) for real,
   not as a toy example.

## Do

Build and run both benchmarks:

```bash
cd code/05-file-copy
make test
```

This generates a 64MB random test file, copies it with `io_uring_cp` and
with `plain_cp`, verifies both copies are byte-identical to the source
(`cmp`), then runs the random-read benchmark and prints both sets of
numbers. Read the actual output, not just this page's summary — your
numbers on your machine will differ from what's written above and in the
sample's README, and that's expected; the direction (roughly even on
sequential, dramatically faster on random) should hold.

Then:

- Change `CHUNK_SIZE` in `io_uring_cp.c` and `QUEUE_DEPTH` in
  `random_read_bench.c` and see how each benchmark's numbers move. Does a
  larger in-flight window help the sequential copy at all? Does it help
  the random-read benchmark linearly, or does the benefit taper off?
- The sample uses buffered I/O throughout (no `O_DIRECT`). `O_DIRECT`
  bypasses the page cache entirely — try adding it to one of the samples
  (you'll need to align buffers and I/O sizes to the filesystem's block
  size) and see whether the sequential-copy result changes once the page
  cache's "free" readahead is out of the picture.

## What this stage doesn't cover

**io_uring cmd passthrough for NVMe** (`IORING_OP_URING_CMD` against a raw
NVMe device, bypassing the block layer) is real, genuinely interesting, and
out of scope here — verifying it honestly needs real NVMe hardware with
passthrough support, which this repository's build environment (a
container, not bare metal with an NVMe device) doesn't have. If you have
that hardware, `axboe/liburing`'s own `examples/` directory and the
`io_uring_cmd(3)` man page are the place to start; this repository doesn't
claim to cover it.

## Checkpoint

- Why did the sequential copy benchmark *not* show io_uring as clearly
  faster in this repo's testing, while the random-read benchmark showed an
  order-of-magnitude difference? Explain it in terms of what each access
  pattern does or doesn't let the kernel's readahead do.
- What does `IOSQE_IO_LINK` buy `io_uring_cp.c` specifically, compared to
  submitting each chunk's read and write as two independent, unlinked
  operations?
- If you were choosing whether to invest in an io_uring-based rewrite of a
  real storage-heavy service, what would you want to measure about its
  actual access pattern *before* assuming the rewrite would pay off?

Next: [Stage 6 — SQPOLL](06-sqpoll.md).
