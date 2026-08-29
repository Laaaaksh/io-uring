# Stage 6 — SQPOLL

**You'll be able to:** explain what `IORING_SETUP_SQPOLL` actually changes,
what it costs, and correctly predict — then measure — whether it helps a
given workload.

**Time:** 1–2 hours.

**Build:** [`code/06-sqpoll`](../code/06-sqpoll).

## What SQPOLL is

Every plain io_uring submission eventually needs an `io_uring_enter()`
syscall to tell the kernel new work is ready (liburing's
`io_uring_submit()` makes this call for you). `IORING_SETUP_SQPOLL` starts
a dedicated kernel thread that polls the submission queue itself — as long
as that thread is still awake, publishing a new SQE (writing it into the
ring and bumping the tail) is enough; no syscall required. liburing's
`io_uring_submit()` already knows how to take advantage of this: it checks
the ring's `IORING_SQ_NEED_WAKEUP` flag and only makes the syscall if the
poll thread has actually gone to sleep, so your submission code doesn't
change at all between a plain ring and an SQPOLL one — only the ring-setup
flags do.

The cost is real and not optional: that kernel thread spins, polling,
burning a full CPU core for as long as it stays awake (tunable via
`sq_thread_idle`, but never zero). SQPOLL trades a syscall for a dedicated
core — worth it exactly when your workload's syscall rate is high enough,
and your CPU budget has a core to spare.

## Read or watch, in this order

1. **[`io_uring_setup(2)`](https://man7.org/linux/man-pages/man2/io_uring_setup.2.html)**
   — the `IORING_SETUP_SQPOLL` and `sq_thread_idle` sections specifically.
2. The privilege-requirement history is worth knowing even though it won't
   affect you on a current kernel: SQPOLL needed `CAP_SYS_ADMIN` through
   Linux 5.11, `CAP_SYS_NICE` from 5.11, and has needed no special
   privileges at all since 5.13. If you're running something older, expect
   an `EPERM` here for a different reason than
   [Stage 7](07-security.md)'s seccomp story.

## Do

Build and run [`code/06-sqpoll`](../code/06-sqpoll)'s benchmark a few times:

```bash
cd code/06-sqpoll
make
./sqpoll_demo.out
```

It runs the same 20,000-op, 64-in-flight workload (tiny reads of a cached
file) on a plain ring and on an SQPOLL ring, and reports which was faster.
Read the README's numbers from this repo's own testing before assuming a
direction: across repeated runs, SQPOLL came out roughly even with the
plain ring, sometimes slightly ahead, sometimes slightly behind — on a
workload this small and this cheap per-syscall, the win didn't clearly
show up. This is not the sample being broken; it's an honest result,
consistent with the trade-off above (SQPOLL's benefit scales with how
expensive your `io_uring_enter()` calls actually are relative to your total
work — and a tiny cached read's syscall overhead is already small).

Then:

- Increase `NUM_OPS` and decrease the per-op work (or point the benchmark
  at a genuinely slow resource, if you have one available) to see whether
  a workload with more syscalls relative to actual work-time shows SQPOLL
  pulling ahead more clearly.
- Read the comment on `get_sqe_wait()` in `sqpoll_demo.c` — an earlier
  version of this exact sample crashed under SQPOLL specifically, because
  the polling thread's own consumption schedule let the submission queue
  fill up in a way a plain ring's synchronous-enough processing didn't
  trigger. This is a real example of SQPOLL surfacing backpressure a plain
  ring's timing happened to hide — see
  [`resources/common-pitfalls.md`](../resources/common-pitfalls.md).

## Checkpoint

- What specifically does `sq_thread_idle` control, and what happens once
  it elapses with no new work?
- Why does `io_uring_submit()` not need a different code path for an
  SQPOLL ring versus a plain one?
- This stage's benchmark didn't show SQPOLL clearly winning. Design (in
  words, you don't have to build it) a workload where you'd expect it to.

Next: [Stage 7 — security](07-security.md).
