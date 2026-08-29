# 06 — SQPOLL

Runs the same 20,000-op, 64-in-flight workload (small reads of a cached
file) on a plain ring and on an `IORING_SETUP_SQPOLL` ring, using
identical `io_uring_submit()` calls in both cases, and times both.

Companion to [`curriculum/06-sqpoll.md`](../../curriculum/06-sqpoll.md).

## Build and run

```bash
make
./sqpoll_demo.out
```

## What was measured

Real output from this repository's own testing, three separate runs:

```
20000 single-read ops, 64 in flight at a time
plain ring:     13.35 ms  (1498144 ops/sec)
SQPOLL ring:    14.37 ms  (1391777 ops/sec)
SQPOLL was NOT faster in this run.

plain ring:     27.87 ms  (717564 ops/sec)
SQPOLL ring:    25.67 ms  (779241 ops/sec)
SQPOLL was 1.1x faster in this run.

plain ring:     69.29 ms  (288639 ops/sec)
SQPOLL ring:   115.22 ms  (173575 ops/sec)
SQPOLL was NOT faster in this run.
```

SQPOLL was roughly a wash across these runs — sometimes marginally ahead,
once clearly behind. This is an honest result, not a broken sample: this
workload's per-syscall cost is already small (a cached read of a tiny
file), so the syscall SQPOLL avoids isn't expensive enough, relative to
everything else happening, for its avoidance to show up clearly against
the cost of running a dedicated polling thread at all. See
`curriculum/06-sqpoll.md` for what workload shape would be expected to show
SQPOLL's benefit more clearly.

## The bug this sample's own development hit

An earlier version of this benchmark submitted all 5,000+ operations
before waiting for any completions, unwindowed. It crashed —
`io_uring_get_sqe()` returned `NULL` (the ring was full of SQEs the kernel
hadn't consumed yet) and the code dereferenced it without checking. This
is real backpressure, not a rare edge case: it's more likely to actually
happen under SQPOLL specifically, because the polling kernel thread
consumes submissions on its own schedule instead of synchronously inside
your submit call, and a fast userspace loop can outrun it. The current
code uses a sliding window (`WINDOW` ops in flight, refilled as each
completes) and a `get_sqe_wait()` helper that retries instead of crashing
when the ring is genuinely full — see the comments in `sqpoll_demo.c` for
the full explanation.

## What to look for

- `sq_thread_idle` is set to 30 seconds in this sample specifically so the
  polling thread stays awake for the whole benchmark — a real deployment
  would tune this against actual request-rate gaps, not benchmark
  convenience. Lower it and watch what happens once the thread goes idle
  mid-run (it has to be woken back up with an actual syscall, the exact
  cost SQPOLL exists to avoid).
- SQPOLL's real cost isn't in this benchmark's numbers at all: the polling
  thread spins on a full CPU core for as long as it stays awake, whether
  or not any work arrives. `nproc`/a process monitor while this runs will
  show it.
