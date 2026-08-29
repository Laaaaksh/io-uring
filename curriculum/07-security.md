# Stage 7 — Security

**You'll be able to:** explain, with real citations, why io_uring is
sandboxed by default in more places than most kernel interfaces, and
diagnose — not just work around — why it might be blocked in a given
environment.

**Time:** 1–2 hours.

**Build:** [`code/07-security`](../code/07-security).

## Why io_uring specifically

This isn't a generic "here's how to sandbox things" chapter. io_uring has a
specific, well-documented security history that's directly relevant to
whether and how you use it in production:

- Google's own security team reported that **io_uring accounted for 60% of
  the kernel-exploit payouts in their VRP program — roughly $1M** — and
  responded by disabling io_uring on production servers and restricting it
  via SELinux on Android (["Learnings from kCTF VRP's 42 Linux kernel
  exploits submissions"](https://security.googleblog.com/2023/06/learnings-from-kctf-vrp-42-linux-kernel-exploits-submissions.html),
  Google Online Security Blog, 2023-06).
- This isn't a closed, historical problem. [CVE-2026-43174](https://www.sentinelone.com/vulnerability-database/cve-2026-43174/)
  (published 2026-05-06) is a use-after-free in io_uring's zero-copy-receive
  subsystem — the same feature area [Stage 4](04-networking.md) touches on
  — showing the pattern is still live, not something that stopped after
  2023.
- The reason keeps repeating across these CVEs: io_uring's object-lifetime
  model (rings, registered files/buffers, and now zero-copy page pools all
  outliving or being freed independently of the operations referencing
  them) is a genuinely harder surface to get right than a simple
  synchronous syscall, and the kernel's own attack surface here keeps
  growing as new zero-copy and passthrough features ship.

The practical consequence: **io_uring is disabled or restricted by default
in more places than you'd expect**, and the failure looks like a
permissions problem, not a missing-feature problem. This repository's own
build process hit this directly — see below.

## Read or watch, in this order

1. **["Learnings from kCTF VRP's 42 Linux kernel exploits submissions"](https://security.googleblog.com/2023/06/learnings-from-kctf-vrp-42-linux-kernel-exploits-submissions.html)**
   (Google Online Security Blog, 2023-06) — read this one in full, it's
   short and it's the primary source for the 60%/$1M figures above.
2. **[`moby/moby#46762`](https://github.com/moby/moby/pull/46762)** — the
   actual PR that made Docker's default seccomp profile block the io_uring
   syscalls (merged 2023-11-02, shipped in Docker 25.0). Skim the diff and
   the discussion; [`moby/moby#47532`](https://github.com/moby/moby/issues/47532),
   a request to reverse it, was closed "not planned" — read that too, it's
   short and it's the maintainers' own reasoning for keeping the block.
3. **["Task-level io_uring restrictions"](https://lwn.net/Articles/1054225/)**
   (LWN.net, 2026-01-19) — the most current entry here: Jens Axboe's
   in-progress work to let `seccomp`-style filtering actually see and
   restrict io_uring operations at a finer grain than "allow everything or
   block everything," which is the state of the art as of this writing.
   Not in a released kernel yet — worth checking back on.

## Do

Run [`code/07-security/seccomp_check.out`](../code/07-security) in three
different environments and compare its output — this is the hands-on
core of this stage, and every one of these was actually run to write this
sentence:

1. **A default Docker container** (`docker run --rm ... `, no
   `--security-opt`): `EPERM`, correctly diagnosed by the tool as a likely
   seccomp block.
2. **The same container with `--security-opt seccomp=unconfined`**:
   `SUCCESS` — this is what every other sample in this repository was
   actually built and run against, and the root [README](../README.md)
   says so explicitly.
3. **Directly on a Linux host or VM with no container involved** (if you
   have one): should also succeed, since there's no seccomp layer to hit at
   all — containers, not virtualization itself, are what triggers this.

Then read `/proc/sys/kernel/io_uring_disabled` (the tool prints it) on
whatever system you're using. Its three values — `0` (default, enabled),
`1` (restricted to `CAP_SYS_ADMIN`), `2` (disabled even for root) — are a
*separate* mechanism from container seccomp policy, and the tool's job is
telling you which one you actually hit.

## Checkpoint

- Google's blog post reports io_uring as 60% of *kernel-exploit* VRP
  payouts. What does that figure not tell you about io_uring's relative
  risk compared to other kernel subsystems? (Hint: what's the denominator,
  and what does "60% of successful kernel exploits" imply about where
  security researchers were focusing their effort in the first place?)
- Why does `EPERM` from a seccomp block and `EPERM` from
  `io_uring_disabled=1` look identical from `errno` alone? How does
  `code/07-security/seccomp_check.c` actually tell them apart?
- If you were deploying a service that uses io_uring into a container
  platform you don't fully control, what would you need to check or
  request, based on everything in this stage?

Next: [Stage 8 — capstone](08-capstone.md).
