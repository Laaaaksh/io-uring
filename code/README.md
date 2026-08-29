# Code samples

Eight self-contained C programs (plus a few comparison baselines), each
isolating one idea. Every sample directory has its own `Makefile` and
`README.md` explaining what to look for and what was actually measured
when this repository was built.

| # | Sample | Idea |
|---|--------|------|
| [01](01-raw-syscall) | `raw_read.c` | The SQ/CQ ring model, with no library — raw syscalls and hand-rolled mmaps |
| [02](02-liburing-basics) | `liburing_basics.c` | prep/submit/wait; a linked SQE chain's failure mode |
| [03](03-fixed-files-and-buffers) | `fixed_vs_plain.c` | Registered files/buffers, measured against plain ones |
| [04](04-echo-server) | `uring_echo.c` | A multishot-accept TCP server; readiness (epoll) vs. completion (io_uring) |
| [05](05-file-copy) | `io_uring_cp.c`, `random_read_bench.c` | In-flight I/O depth — measured to help a lot on random reads, not much on sequential |
| [06](06-sqpoll) | `sqpoll_demo.c` | SQPOLL's syscall-avoidance vs. CPU cost, measured |
| [07](07-security) | `seccomp_check.c` | Diagnosing why io_uring is blocked somewhere, hands-on |
| [08](08-capstone) | `uring_srv.c`, `epoll_srv.c`, `blocking_srv.c` | All three concurrency models, same workload, same benchmark |

Each pairs with a stage in [`curriculum/`](../curriculum) — see the
"Companion to" link at the top of each sample's source file.

## Building

Every sample builds and runs the same way:

```bash
cd code/0N-sample-name
make        # compiles
make run    # or: make test — runs it; test targets are deterministic, CI-checked
```

## How this was verified

**Every sample in this directory was actually run, not just compiled** —
the numbers in each README are real output from this repository's own
environment, captured while building it. That environment:

```bash
docker run --rm -it --security-opt seccomp=unconfined \
  -v "$PWD:/work" -w /work ubuntu:24.04
# then, inside the container:
apt update && apt install -y liburing-dev gcc make
```

- **Kernel:** 6.12.76-linuxkit (Docker Desktop's own Linux VM, Apple
  Silicon host, arm64) — a real Linux kernel, not an emulation of one. See
  [`curriculum/00-toolchain-setup.md`](../curriculum/00-toolchain-setup.md)
  for why `--security-opt seccomp=unconfined` is necessary here and what it
  means.
- **liburing:** 2.5-1build1 (Ubuntu 24.04's `apt` package) — meaningfully
  older than the current upstream release (2.15, 2026-06-29); see
  [`resources/curated-resources.md`](../resources/curated-resources.md).
- **gcc:** 13.3.0, all samples compiled with `-Wall -Wextra`, zero warnings.
- **CI** (`.github/workflows/ci.yml`) builds and runs every sample's `test`
  target on GitHub's own `ubuntu-latest` runners on every push and PR — a
  real Linux VM, not a seccomp-restricted container, so no
  `--security-opt` workaround is needed there.

Benchmark numbers throughout this repository (the fixed-buffer comparison,
the SQPOLL comparison, the sequential-vs-random storage comparison, the
capstone's three-way server comparison) are real measurements from this
environment, run multiple times, with the actual range reported — not
single cherry-picked runs, and not asserted claims. Where a result varies
between runs (several do — this is a shared, virtualized environment, not
dedicated hardware), each sample's README says so and gives the range
actually observed.

## `code/common/`

[`common.h`](common/common.h) has two macros every sample uses:
`CHECK_RC` for liburing/raw-syscall calls that return `-errno` directly on
failure, and `CHECK_ERRNO` for ordinary libc calls that return `-1` and set
`errno`. Using the wrong one silently misreports the error, so every sample
is consistent about which is which — see the comment in the header for the
distinction.
