# Stage 1 — The mental model

**You'll be able to:** explain what a submission queue and completion queue
actually are (shared memory, not a queue of syscalls), and submit one
operation through io_uring with no library at all.

**Time:** 2–3 hours.

**Build:** [`code/01-raw-syscall`](../code/01-raw-syscall) — build and run
it.

## Why raw syscalls, once

Every other sample in this repository uses liburing, because that's what
real code should use. This one stage deliberately doesn't, for the same
reason `cuda-engineering` (the first repository in this series) has you
write a kernel with no framework before reaching for anything higher-level:
liburing's `io_uring_prep_read()` and `io_uring_submit()` are genuinely
simple wrappers around exactly the syscalls and shared-memory rings you'll
build by hand in this stage. Seeing what they wrap makes every later stage
something you understand rather than something you trust.

## Read or watch, in this order

1. **[io_uring(7)](https://man7.org/linux/man-pages/man7/io_uring.7.html)**
   (man7.org, HTML rendered 2026-05-30) — the concept overview, including a
   complete example program. Read the whole thing before touching code;
   it's short.
2. **["Efficient IO with io_uring"](https://kernel.dk/io_uring.pdf)** — Jens
   Axboe's original design rationale. Covers *why* Linux's older `aio`
   interface wasn't good enough (`O_DIRECT`-only, unpredictable blocking,
   an API that copies 104 bytes per op for something "supposedly
   zero-copy") and what io_uring's three design goals were. Read this
   *after* the man page, not before — it lands better once you've seen the
   shape of the actual interface.
3. **[`io_uring_setup(2)`](https://man7.org/linux/man-pages/man2/io_uring_setup.2.html)**
   — the exact syscall [`code/01-raw-syscall`](../code/01-raw-syscall) calls
   directly. Skim it for the `io_uring_params` struct and the
   `IORING_FEAT_*` flags; you'll recognize both in the code.

## Do

Build and run [`code/01-raw-syscall`](../code/01-raw-syscall). Read the code
alongside the man page — every mmap call and every ring-index calculation
in it corresponds to something the man page describes in prose. Then:

- Change the SQE's `user_data` value and confirm the completion echoes back
  whatever you set — this is the mechanism every later sample uses to know
  *which* in-flight operation a completion belongs to, once there's more
  than one in flight at a time.
- Comment out the `__atomic_store_n(sq_tail, ...)` line that publishes the
  new SQE and re-run. The program should hang (or fail differently) — the
  kernel never sees an SQE you claimed a slot for but never announced via
  the tail. This is the two-step "write the slot, then bump the tail"
  protocol every shared ring in this repository depends on, made visible by
  breaking it on purpose.

## Checkpoint

Answer these without looking anything up:

- What two things does `io_uring_setup()` actually give you back, and what
  does each mmap in the code correspond to?
- Why does the code check `IORING_FEAT_SINGLE_MMAP` before proceeding,
  instead of always doing two separate mmaps?
- The completion queue's `user_data` field is just a `u64` this sample sets
  to `0x1234`. What would you put there instead in a program juggling
  multiple in-flight operations at once — and why can't the kernel just
  tell you "which struct this belongs to" on its own?

Next: [Stage 2 — liburing basics](02-liburing-basics.md).
