# The things that actually stop people

Collected in one place, so you can check here before assuming you've found
a new problem. Every one of these was hit for real while building this
repository — not a hypothetical list. Each is covered in more depth
wherever the curriculum stage links to it; this page is the index of
"oh, it's this again."

## Toolchain and versions

**io_uring silently fails inside Docker with `EPERM` or `ENOSYS`.** This is
the single most common way this repository's own samples failed while
building it, and it isn't a bug in your code. Docker's *default* seccomp
profile has blocked `io_uring_setup`/`io_uring_enter`/`io_uring_register`
since Docker 25.0 ([`moby/moby#46762`](https://github.com/moby/moby/pull/46762),
merged 2023-11-02) — a deliberate response to io_uring's CVE history, not an
oversight (a request to reverse it,
[`moby/moby#47532`](https://github.com/moby/moby/issues/47532), was closed
"not planned"). Run `code/07-security/seccomp_check.out` to diagnose which
of three things is actually going on (old kernel, seccomp block, or the
`io_uring_disabled` sysctl); fix for local development is
`docker run --security-opt seccomp=unconfined ...`, which is what this
repo's own samples were built and run against — see the root
[README](../README.md) and [Stage 7](../curriculum/07-security.md).

**`apt`'s liburing is older than you think.** Ubuntu 24.04's `liburing-dev`
package is version **2.5**; the current upstream release is **2.15**
(2026-06-29, see [`resources/curated-resources.md`](curated-resources.md)).
Every sample in this repo was built and verified against 2.5 specifically —
if a liburing feature you read about elsewhere isn't compiling, check
`pkg-config --modversion liburing` before assuming your code is wrong. See
[Stage 0](../curriculum/00-toolchain-setup.md) for how to build a newer
liburing from source if you need one.

**A kernel too old for the op you're using.** io_uring itself needs 5.1+;
individual operations need much more recent kernels — multishot accept
needs 5.19, multishot recv needs 6.0, `IORING_SETUP_SINGLE_ISSUER` needs
6.1. If `io_uring_prep_multishot_accept` compiles (it's just a liburing
function) but every completion comes back `-EINVAL`, check `uname -r`
against the op you're using — liburing doesn't hide a kernel that's too old
for a specific opcode. [Stage 0](../curriculum/00-toolchain-setup.md) has
the version table.

## Multishot operations and use-after-free

**Freeing your `user_data` after the first completion of a multishot op.**
This is a real bug this repository's own [`code/04-echo-server`](../code/04-echo-server)
had during development: a multishot accept SQE is submitted *once* but
produces a completion for *every* incoming connection, and every one of
those completions carries the same `user_data` pointer you set at
submission time. Free that pointer after the first completion (a natural
thing to do if you're used to one-completion-per-SQE ops) and every later
completion for that same multishot SQE points at freed memory — a
use-after-free that showed up as a segfault with a garbled `op->type` field
in this repo's own testing. Only free the tag once `cqe->flags &
IORING_CQE_F_MORE` is *unset*, meaning the kernel is actually done with
that SQE. See the comment in
[`code/04-echo-server/uring_echo.c`](../code/04-echo-server/uring_echo.c)
for the exact fix.

## Backpressure

**Assuming `io_uring_get_sqe()` always succeeds.** It returns `NULL` when
the ring already holds as many SQEs as the kernel hasn't consumed yet — real
backpressure, not an error condition to ignore. This is more likely to bite
under `IORING_SETUP_SQPOLL` than a plain ring, because the polling kernel
thread consumes submissions on its own schedule instead of synchronously
inside your submit call, and a fast userspace loop can outrun it — exactly
what happened building [`code/06-sqpoll`](../code/06-sqpoll)'s benchmark.
The fix is the same as any full queue: check for `NULL` and wait (flush
with `io_uring_submit()`, which is a harmless no-op if nothing's pending,
and retry) rather than dereferencing it.

## Measuring, not assuming

**"io_uring is faster" isn't automatically true for your workload.** Two of
this repo's own samples measured the opposite of the common narrative:
[`code/05-file-copy`](../code/05-file-copy)'s sequential copy of a
page-cache-resident file was *not* faster with io_uring than a plain
`read()`/`write()` loop (the kernel's own readahead already pipelines
sequential access "for free"), and
[`code/08-capstone`](../code/08-capstone)'s naive echo-server implementation
was, if anything, marginally slower than epoll on localhost with trivial
per-connection latency. Both samples' READMEs show the real numbers and
explain why. io_uring's advantage is concrete and large where there's real
I/O latency to hide behind concurrency — see
[`code/05-file-copy`](../code/05-file-copy)'s random-read benchmark, which
measured an 11–23× speedup in this repo's own testing — but "io_uring" is
not a synonym for "faster," and this repo's own numbers are the proof.

## Misconceptions almost everyone starts with

**"io_uring replaces epoll."** It can, but they answer different
questions. epoll tells you an fd is *ready*; you still call `read()`/
`write()` yourself. io_uring's read/write/accept/send/recv operations are
themselves queued and completed asynchronously — you get told the *result*,
not just readiness. [Stage 4](../curriculum/04-networking.md) puts a real
epoll server and a real io_uring server side by side so the difference is
something you can read in two actual source files, not just a sentence.

**"Registering a file or buffer is free performance."** It removes real
per-operation costs (fd-table lookup, buffer pinning) — but on a workload
that's already cheap per operation, that saving can be smaller than the
noise floor of a shared, virtualized benchmark environment.
[`code/03-fixed-files-and-buffers`](../code/03-fixed-files-and-buffers)'s
README shows exactly this: a real, measured, sometimes-negative result
alongside an honest explanation of when the mechanism matters more.

**"A linked SQE chain runs the second op no matter what."**
`IOSQE_IO_LINK` means the opposite of "run regardless" — if the first op in
a linked chain fails, every SQE linked after it is completed with
`-ECANCELED` instead of running at all.
[`code/02-liburing-basics`](../code/02-liburing-basics) demonstrates this
by deliberately breaking the first op in a chain and showing the second
one's completion.

**"SQPOLL is free concurrency."** It trades a syscall on submission for a
kernel thread that spins polling the submission queue — a full CPU core,
for as long as the thread stays awake (tunable via `sq_thread_idle`, but
never zero cost). [`code/06-sqpoll`](../code/06-sqpoll)'s README covers
this trade-off with real numbers from this repo's own testing, including a
case where SQPOLL was *not* faster.
