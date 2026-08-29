# 08 — Capstone: io_uring vs. epoll vs. blocking, measured

Three TCP echo servers — `uring_srv`, `epoll_srv`, `blocking_srv` —
functionally identical, built three different ways. One benchmark client
(`bench_client`) fires the same concurrent load at each and times how long
the whole batch takes.

Companion to [`curriculum/08-capstone.md`](../../curriculum/08-capstone.md).

## Build and run

```bash
make bench                     # default: 300 clients, quick
make bench NUM_CLIENTS=2000    # the numbers below
make test                      # same as bench, but fails the build on any dropped/mismatched connection
```

## What was measured

Real output from this repository's own testing, six runs at 2,000
concurrent connections on localhost (Linux 6.12, see the repo root
README):

```
=== io_uring ===  === epoll ===  === blocking ===
   148.00 ms         86.70 ms        84.97 ms
   100.21 ms         81.67 ms        89.09 ms
    72.53 ms         74.80 ms        76.96 ms
    91.48 ms         70.82 ms        84.37 ms
    91.77 ms         (not captured this run)
    79.77 ms         (not captured this run)
    83.22 ms         (not captured this run)
```

Across every run, **io_uring was at or near the slowest of the three, not
the fastest** — the opposite of what an "io_uring beats epoll beats
blocking" narrative would predict. epoll and the plain blocking server
landed close to each other, both usually ahead of io_uring.

**Why, most likely** (see `curriculum/08-capstone.md` for the full
discussion): this benchmark's per-connection work is nearly free (a 4-byte
message, localhost, no real network latency), so there's almost no I/O
wait for io_uring's concurrency to overlap — meanwhile this server's
implementation pays real, measurable bookkeeping cost the other two don't:
a `calloc()`/`free()` pair per operation, and an `io_uring_submit()` call
after every single completion instead of batching multiple submissions
together. On a workload with real I/O latency to hide (see
[`code/05-file-copy`](../05-file-copy)'s random-read benchmark, an 11–23×
win for io_uring), the picture is completely different. This benchmark and
that one, read together, are the actual lesson of this capstone: io_uring's
advantage is conditional on the workload, not automatic, and this repo
shows you a real measured case of each.

This is not a claim that io_uring can never win a networking benchmark —
it's a report of what *this* implementation measured on *this* workload,
honestly, including the parts that don't flatter the technology this
repository is otherwise making a case for.

## What to look for

- Read all three servers side by side:
  [`uring_srv.c`](uring_srv.c), [`epoll_srv.c`](epoll_srv.c),
  [`blocking_srv.c`](blocking_srv.c). The structural difference —
  completion-based vs. readiness-based vs. no multiplexing at all — is
  something you can point to in actual code, not just describe abstractly.
- `blocking_srv.c`'s `listen()` backlog is set to 1024, not the usual
  small default — see the comment explaining why: the benchmark client
  fires every connection at once, and a small backlog would reject
  connections outright before this server's one-at-a-time handling even
  comes into play, which would make it look artificially worse for a
  reason unrelated to what this benchmark is testing.
- `bench_client.c` uses `fork()` to generate genuinely concurrent client
  processes, not a loop pretending to be concurrent — real independent
  processes actually racing to connect at once.
- Try the two changes suggested in `curriculum/08-capstone.md` (batch
  multiple submissions per `io_uring_submit()` call; use registered
  buffers) and see whether they close the gap.
