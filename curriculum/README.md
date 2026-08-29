# The curriculum

Nine stages, in order. Each one names what you'll be able to do at the end,
what to read or watch, roughly how long it takes, and what to build to prove
it stuck. Do them in order the first time through - later stages assume
earlier ones, and the code samples get more involved as they go.

| Stage | You'll be able to... | Time | Build |
|---|---|---|---|
| [0 — Toolchain setup](00-toolchain-setup.md) | Get a Linux environment where io_uring actually works, and prove it | 30–60 min | `code/07-security/seccomp_check.out` succeeds |
| [1 — Mental model](01-mental-model.md) | Explain the SQ/CQ ring model; submit one op with no library at all | 2–3 hr | [`code/01-raw-syscall`](../code/01-raw-syscall) |
| [2 — liburing basics](02-liburing-basics.md) | Use prep/submit/wait; explain what a linked SQE chain does on failure | 2–3 hr | [`code/02-liburing-basics`](../code/02-liburing-basics) |
| [3 — Fixed files & buffers](03-fixed-files-and-buffers.md) | Register files/buffers; explain what that saves and when it's worth it | 1–2 hr | [`code/03-fixed-files-and-buffers`](../code/03-fixed-files-and-buffers) |
| [4 — Networking](04-networking.md) | Build a multishot-accept io_uring TCP server; explain it against epoll | 3–4 hr | [`code/04-echo-server`](../code/04-echo-server) |
| [5 — Storage](05-storage.md) | Pipeline storage I/O with in-flight depth; know when that helps and when it doesn't | 3–4 hr | [`code/05-file-copy`](../code/05-file-copy) |
| [6 — SQPOLL](06-sqpoll.md) | Explain the syscall-avoidance vs. CPU-cost trade-off; measure it yourself | 1–2 hr | [`code/06-sqpoll`](../code/06-sqpoll) |
| [7 — Security](07-security.md) | Explain why io_uring is sandboxed by default and diagnose why it's blocked somewhere | 1–2 hr | [`code/07-security`](../code/07-security) |
| [8 — Capstone](08-capstone.md) | Reason about when to reach for io_uring at all, backed by your own measurements | 2–3 hr | [`code/08-capstone`](../code/08-capstone) |

**Total: roughly 16–24 hours** of focused work, spread over however long
that takes you. There's no clock running.

## Prerequisites

You should already be comfortable reading and writing C, and know your way
around basic POSIX I/O (`open`/`read`/`write`/`close`) and basic sockets
(`socket`/`bind`/`listen`/`accept`). You do **not** need prior async I/O,
event-loop, or kernel-internals experience — Stage 1 builds the mental
model from the raw syscalls up. If you've never used `epoll` before,
[Stage 4](04-networking.md) explains what it does before comparing io_uring
against it.

## How each stage is structured

- **What to read or watch** — a short, sequenced list, not an unordered
  pile. Full annotated details (what's covered well, how current it is, and
  what's stale but still worth it) live in
  [`resources/curated-resources.md`](../resources/curated-resources.md) —
  each stage links the specific entries relevant to it.
- **What to build** — a runnable, commented C program in
  [`code/`](../code), with its own README explaining what to look for and
  what was actually measured when this repo was built. Building it is not
  optional — io_uring's failure modes (a use-after-free on a multishot
  completion, backpressure on a full submission queue, a linked chain
  canceling silently) are the kind of thing you have to trigger once to
  really believe, not just read about.
- **Checkpoint questions** — answer these without looking anything up
  before moving on. They're not a quiz to pass, they're a way to notice
  what didn't actually land.

## If something stops you

[`resources/common-pitfalls.md`](../resources/common-pitfalls.md) collects
the specific things that stopped this repository's own author while
building it — toolchain and seccomp gotchas, a real use-after-free bug, a
real backpressure bug, and the misconceptions almost everyone starts with.
Check there before assuming you've found a new problem.

## Where this curriculum stops

This is single-machine, single-process io_uring: the syscall interface,
liburing, and the operations most people actually need (file and network
I/O, registered resources, SQPOLL). It does **not** cover:

- **io_uring cmd passthrough for NVMe** (`IORING_OP_URING_CMD` against a raw
  NVMe device) — needs real NVMe hardware with passthrough support to
  verify honestly, which this repo's build environment doesn't have. See
  [Stage 5](05-storage.md) for what's covered instead and where to go for
  this.
- **eBPF integration with io_uring** — a real, separate, deep topic. If
  you're specifically after eBPF, [`lizrice/learning-ebpf`](https://github.com/lizrice/learning-ebpf)
  is a genuinely good, actively maintained resource — see this research's
  source material for why that gap is already closed.
- **Kernel-internals-level io_uring development** (writing new opcodes,
  contributing to the kernel side) — this curriculum is written for
  *applications* that use io_uring, not for kernel developers extending it.
- **Language-runtime-specific bindings** beyond raw C/liburing (Rust's
  `tokio-uring`/`glommio`/`monoio`, Go bindings, etc.) — [Stage 8](08-capstone.md)'s
  "where this sits now" section names the actively maintained ones and
  where to go next, but doesn't teach them.
