# Stage 8 — Capstone: when to actually reach for this

**You'll be able to:** compare io_uring against epoll and against plain
blocking I/O on the same real workload, with your own numbers — and know
where the wider ecosystem stands today, so you can decide when io_uring is
the right tool versus when something built on it (or nothing at all) is.

**Time:** 2–3 hours.

**Build:** [`code/08-capstone`](../code/08-capstone).

## The benchmark

Three TCP echo servers, functionally identical, built three different
ways:

- **`uring_srv`** — the io_uring server from [Stage 4](04-networking.md),
  multishot accept and per-connection recv/send.
- **`epoll_srv`** — the same server on epoll: one epoll instance, readiness
  notifications, synchronous `read()`/`write()` once notified.
- **`blocking_srv`** — no multiplexing at all: `accept()`, `read()`,
  `write()`, `close()`, one connection at a time, in a loop.

A benchmark client fires many connections at once (via `fork()`, so they're
genuinely concurrent processes) and times how long the whole batch takes
each server to handle.

**What this repository's own testing found, at 2,000 concurrent
connections on localhost, repeated across six runs:** the three servers
landed within roughly 20% of each other, with **io_uring at or near the
slowest of the three in most runs**, not the fastest. This is the opposite
of what a "io_uring beats epoll beats blocking" narrative would predict —
and it's a real, reproduced measurement, not a fluke from one run. See
[`code/08-capstone`](../code/08-capstone)'s README for the actual numbers
and the most likely explanation: this server's implementation allocates a
small struct per operation and calls `io_uring_submit()` after every single
completion, and on localhost with one tiny message per connection, there's
almost no I/O wait to hide behind that concurrency in the first place —
so io_uring pays real per-operation bookkeeping cost without much latency
to overlap it against. Compare this against
[Stage 5](05-storage.md)'s random-read benchmark, which measured an
11–23× win for io_uring on a workload that genuinely has I/O wait to hide.
Put the two capstone-adjacent results side by side and the lesson is the
same one [`resources/common-pitfalls.md`](../resources/common-pitfalls.md)
states directly: **"io_uring" is not a synonym for "faster."** It's faster
specifically when there's real per-operation latency to overlap, and this
repository shows you both a case where that's true and a case where it
isn't, measured in the same way, so you can tell the difference in your
own work instead of taking either claim on faith.

## Do

```bash
cd code/08-capstone
make bench                    # default: 300 clients, quick
make bench NUM_CLIENTS=2000    # what this stage's numbers above used
```

Read all three servers' source side by side —
[`uring_srv.c`](../code/08-capstone/uring_srv.c),
[`epoll_srv.c`](../code/08-capstone/epoll_srv.c), and
[`blocking_srv.c`](../code/08-capstone/blocking_srv.c) — before drawing any
conclusions from the numbers. The structural difference between "queue an
async operation and get told the result" (io_uring) and "get told
readiness, then do the I/O yourself" (epoll) and "just block" is something
you can point to in actual lines of code once you've built all three.

Then:

- Try to make the io_uring server actually win: batch multiple
  `io_uring_prep_*` calls before a single `io_uring_submit()` instead of
  submitting after every completion, or use registered buffers
  ([Stage 3](03-fixed-files-and-buffers.md)) to remove the per-op
  allocation this implementation currently does. Does either change the
  result?
- Increase the per-connection work (have the benchmark client hold the
  connection open briefly, or send a larger payload) and re-run. Does the
  gap between the three servers change as per-connection cost goes up?

## Where this sits now

- **PostgreSQL 18** (released 2025-09-25) shipped `io_method=io_uring` as
  an opt-in async I/O backend, reporting up to 3× throughput on sequential
  scans and vacuum — real, current, production-grade adoption (defaults to
  `sync`; using io_uring is a deliberate operator choice, not automatic).
- In Rust, the ecosystem has genuinely diverged rather than converged on
  one answer: mainline **Tokio** stays epoll-based by default;
  **[`tokio-rs/tokio-uring`](https://github.com/tokio-rs/tokio-uring)**, the
  most obvious "just add io_uring to Tokio" path, is **stalled** — no push
  in over a year as of this writing, changelog inactive since around 2022.
  What's actually active instead: **[`bytedance/monoio`](https://github.com/bytedance/monoio)**
  (5,082★, pushed 2026-07-20), a thread-per-core runtime built by ByteDance
  specifically around io_uring, and **[`DataDog/glommio`](https://github.com/DataDog/glommio)**
  (3,642★, pushed 2026-04-22), aimed specifically at database/proxy/broker
  workloads. If you're evaluating io_uring in Rust, these two — not
  `tokio-uring` — are where the current activity is.
- The security story from [Stage 7](07-security.md) is actively evolving,
  not settled: task-level restrictions
  ([LWN, 2026-01-19](https://lwn.net/Articles/1054225/)) are in-progress
  kernel work, not yet shipped, aimed at closing the exact seccomp gap this
  repository demonstrated hands-on.

## What this repository doesn't cover, and where to go next

- **NVMe passthrough, eBPF integration, kernel-internals development** —
  see [`curriculum/README.md`](README.md)'s "where this curriculum stops"
  for what's out of scope and why.
- **Building a production-grade async runtime on io_uring** — this
  curriculum gets you to "can build a correct, if unoptimized,
  single-threaded io_uring server and reason about its trade-offs." Going
  from there to something like monoio or glommio's internals (multi-core
  work distribution, more sophisticated buffer management, real production
  hardening) is a substantial further step this repository doesn't attempt.

## Checkpoint — for the whole curriculum, not just this stage

- State, in one paragraph, the specific *kind* of workload where this
  repository's own measurements showed io_uring winning clearly, and the
  kind where they didn't. Be concrete about *why*, not just "it depends."
- If someone on your team proposed rewriting a service to use io_uring,
  what's the first thing you'd ask them to measure about the current
  service, based on everything in this curriculum?
- Name one thing from [Stage 7](07-security.md) you'd want confirmed about
  a deployment target before shipping io_uring-based code to it.

That's the curriculum. [`resources/curated-resources.md`](../resources/curated-resources.md)
has the full annotated resource list if you want to go deeper on any single
stage, and [`resources/common-pitfalls.md`](../resources/common-pitfalls.md)
is worth a second read now that you've hit some of it yourself.
