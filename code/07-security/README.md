# 07 — Security: diagnosing why io_uring is blocked

A small diagnostic tool: try `io_uring_queue_init()`, and if it fails,
tell you *which* of three distinct reasons is actually going on — an old
kernel, a seccomp filter blocking the syscalls, or the `io_uring_disabled`
sysctl — since the fix is different for each.

Companion to [`curriculum/07-security.md`](../../curriculum/07-security.md).

## Build and run

```bash
make
./seccomp_check.out
```

## What was measured — a real three-way comparison

Every one of these was actually run while building this repository, not
described from documentation:

**1. Inside this repo's normal dev environment** (`--security-opt
seccomp=unconfined`, see the repo root README):

```
$ ./seccomp_check.out
io_uring_disabled sysctl = 0 (enabled for everyone (default))
io_uring_queue_init: SUCCESS - io_uring is usable here.
```

**2. Inside a container with Docker's *default* seccomp profile** (no
`--security-opt` flag at all):

```
$ docker run --rm -v "$PWD:/work" -w /work/code/07-security ubuntu:24.04 bash -c \
  'apt-get update -qq && apt-get install -y -qq liburing-dev gcc >/dev/null && \
   gcc -O2 -o /tmp/check seccomp_check.c -luring && /tmp/check'
io_uring_disabled sysctl = 0 (enabled for everyone (default))
io_uring_queue_init: FAILED - Operation not permitted (errno 1)
EPERM most likely means a seccomp filter is blocking the io_uring syscalls
specifically - this is Docker's *default* seccomp profile since Docker 25.0
(see this sample's README for why). It can also mean the io_uring_disabled
sysctl is set to 1 and this process lacks CAP_SYS_ADMIN. Check the sysctl
value printed above to tell which.
```

Same kernel, same liburing, same code — the only difference between run 1
and run 2 is one Docker flag, and the tool correctly identifies why: the
sysctl reads `0` (not restricted) in both cases, so the `EPERM` in run 2
has to be the seccomp filter, exactly as printed.

## What to look for

- The three failure modes this tool distinguishes are genuinely different
  problems with genuinely different fixes: `ENOSYS` usually means a kernel
  too old for io_uring at all (pre-5.1); `EPERM` with
  `io_uring_disabled=0` means a seccomp filter (Docker's default, or a
  custom sandbox); `EPERM`/`ENOSYS` with `io_uring_disabled=1` means an
  admin restricted it system-wide. Confusing these wastes real debugging
  time — this tool exists because that confusion is exactly what happened
  while building this repository.
- This program takes no untrusted input and isn't itself a security
  boundary — it's a diagnostic, not a sandboxing mechanism. See
  [`SECURITY.md`](../../SECURITY.md) for what does and doesn't belong in a
  report about this repository specifically, versus a real kernel
  vulnerability (which goes to the kernel security team, not here).
- Try setting `/proc/sys/kernel/io_uring_disabled` to `1` yourself (as
  root, on a real Linux box you control — **not** recommended on a shared
  machine) and re-run as a non-root user: you should see the
  `io_uring_disabled` line change to `1` and the same `EPERM` failure, now
  correctly attributed by the tool to the sysctl instead of seccomp.
