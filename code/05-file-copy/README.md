# 05 — File copy and random reads

Two benchmarks, one point: io_uring's advantage comes from overlapping
real I/O wait time across many in-flight operations, and how much that
matters depends entirely on your access pattern.

Companion to [`curriculum/05-storage.md`](../../curriculum/05-storage.md).

## Build and run

```bash
make test
```

This generates a 64MB random test file, copies it with `io_uring_cp.out`
(8 chunk-copies in flight, each a linked read+write pair) and with
`plain_cp.out` (a synchronous read/write loop), verifies both copies are
byte-identical to the source, then runs `random_read_bench.out` (20,000
random 4KB reads, one at a time vs. 32 in flight via io_uring).

## What was measured

Real output from this repository's own testing (Linux 6.12, a 64MB file,
same environment as every other sample — see the repo root README):

**Sequential copy** — io_uring was *not* consistently faster, across
repeated runs:

```
./io_uring_cp.out testfile.bin testfile.bin.uring.copy
copied 67108864 bytes in 146.01 ms (438.3 MB/s)
./plain_cp.out testfile.bin testfile.bin.plain.copy
copied 67108864 bytes in 81.37 ms (786.5 MB/s)
```
```
./io_uring_cp.out testfile.bin testfile.bin.uring.copy
copied 67108864 bytes in 84.63 ms (756.2 MB/s)
./plain_cp.out testfile.bin testfile.bin.plain.copy
copied 67108864 bytes in 70.94 ms (902.2 MB/s)
```

Plain, synchronous `read()`/`write()` was faster **in every run**, not
just occasionally. The likely reason: this is sequential access to an
already page-cache-resident file, where the kernel's own readahead already
pipelines reads ahead of the calling thread "for free" — there's very
little I/O wait left for io_uring's explicit concurrency to overlap, so it
only adds bookkeeping cost (SQE prep, CQE processing, linked-chain
tracking) without a matching benefit here.

**Random reads** — io_uring was dramatically faster, consistently:

```
20000 random 4096-byte reads from a 64 MB file
plain pread(), one at a time:          211.49 ms  (94568 reads/sec)
io_uring, 32 in flight:                 18.72 ms  (1068141 reads/sec)
io_uring was 11.3x faster in this run.
```
```
plain pread(), one at a time:          823.31 ms  (24292 reads/sec)
io_uring, 32 in flight:                 35.56 ms  (562460 reads/sec)
io_uring was 23.2x faster in this run.
```

Random access defeats the kernel's readahead prediction — each read is a
genuinely independent operation with its own wait, and whether 32 of them
overlap or run strictly one at a time is the entire difference. This
result was reproducible: an 11–23× speedup across every run in this
repo's own testing, never close to parity.

**Conclusion, stated plainly:** don't extrapolate "io_uring is N× faster"
from one benchmark shape to another. This directory contains two real
benchmarks that disagree with each other for a specific, explainable
reason — that disagreement is the actual lesson.

## What to look for

- `io_uring_cp.c` links each chunk's read to its write with
  `IOSQE_IO_LINK` (see [Stage 2](../../curriculum/02-liburing-basics.md))
  so a write can never run before its read completes and land wrong data.
- Both benchmarks use a **sliding window** (`QUEUE_DEPTH`/`WINDOW`
  in-flight operations, refilled as each completes) rather than submitting
  everything up front — see the comment in `random_read_bench.c` about why
  an earlier, unwindowed version of a related sample
  (`code/06-sqpoll/sqpoll_demo.c`) crashed by ignoring this.
- Neither sample uses `O_DIRECT` — both go through the page cache, on
  purpose, so they run identically on any filesystem including the
  overlay/tmpfs-backed ones common in containers. `curriculum/05-storage.md`
  covers what changes with `O_DIRECT` and why this repo doesn't use it by
  default.
