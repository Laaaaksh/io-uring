# Stage 0 — Get a Linux environment where io_uring actually works

**You'll be able to:** get to a Linux environment with a modern enough
kernel, install liburing, and *prove* io_uring actually works there rather
than assuming it does.

**Time:** 30–60 minutes, mostly installs.

**Build:** [`code/07-security/seccomp_check.out`](../code/07-security) — this
stage ends when it prints `SUCCESS`, not just when a compiler runs.

## Why "it compiles" isn't enough here

Unlike a lot of toolchains, getting io_uring code to *compile* is the easy
part — `gcc` and `liburing-dev` are a normal `apt install` away, on any
Linux box. Getting a real *io_uring_setup() call to succeed at runtime* is
the part that actually stops people, for a reason specific to this
technology: io_uring has enough of a security history (see
[Stage 7](07-security.md)) that some environments block it deliberately,
and the failure looks like a permissions error, not a missing-dependency
error. This repository's own samples hit exactly this while being built —
see below.

## You need Linux, one way or another

io_uring is a Linux kernel interface. There's no macOS or Windows
equivalent, and no userspace emulation of it — you need a real Linux
kernel, though not necessarily bare metal.

### If you're already on Linux

Check your kernel version:

```bash
uname -r
```

You want **5.19 or newer** to follow every sample in this repository
without hitting a missing-opcode error (multishot accept needs 5.19,
multishot recv needs 6.0). This repository itself was built and verified
against **Linux 6.12** — see the root [README](../README.md) for the exact
environment. Anything from a current Ubuntu/Fedora/Debian release should
already clear this bar; WSL2 on Windows also works here, since it runs a
real Linux kernel (check `uname -r` inside WSL2 the same way).

Install the library and a compiler:

```bash
sudo apt install liburing-dev gcc make   # Debian/Ubuntu
```

Check what you got — this matters more here than in most C projects:

```bash
pkg-config --modversion liburing
```

Ubuntu 24.04's package is liburing **2.5**; current upstream is **2.15**
(2026-06-29). This repository was built and verified against 2.5
specifically, and every sample says so. If you need a newer liburing for a
feature Ubuntu's package doesn't have yet, build it from source per
[`axboe/liburing`](https://github.com/axboe/liburing)'s own README — it's a
small, dependency-light build.

### If you're not on Linux (macOS, Windows without WSL2)

You need a Linux VM or container with a real, modern kernel. This
repository itself was built entirely this way — no bare-metal Linux
machine was used or is needed. Two real options, in the order this repo's
author actually used them:

**Docker Desktop's own Linux VM.** If you already have Docker Desktop
installed (macOS or Windows), it's already running a real Linux VM to host
containers — you don't need a separate VM tool. Check what kernel it's
running:

```bash
docker run --rm ubuntu:24.04 uname -r
```

At the time of writing this printed `6.12.76-linuxkit` on Docker Desktop
for Mac (Apple Silicon) — comfortably modern enough.

**A cloud VM or a dedicated VM tool** (UTM, Multipass, a $5/mo cloud
instance, GitHub Codespaces) if you'd rather not route everything through
Docker. Any of these gives you a real Linux kernel the same way; the
seccomp gotcha below is specific to *containers*, not VMs, so a real VM
sidesteps it entirely.

## The gotcha that will actually stop you: Docker's default seccomp profile

This is the single most time-costly thing this repository's own author hit
while building it, and it's worth understanding rather than just working
around blindly.

Running io_uring code in a **default** Docker container fails, even on a
kernel that fully supports it:

```bash
$ docker run --rm ubuntu:24.04 bash -c "apt update -qq && apt install -y liburing-dev gcc >/dev/null && \
    echo 'int main(){}' > t.c && gcc t.c -o t -luring 2>/dev/null; ./code/07-security/seccomp_check.out"
io_uring_disabled sysctl = 0 (enabled for everyone (default))
io_uring_queue_init: FAILED - Operation not permitted (errno 1)
EPERM most likely means a seccomp filter is blocking the io_uring
syscalls specifically...
```

This is **not** a bug, and it's **not** the `io_uring_disabled` sysctl
(which the diagnostic above correctly shows as `0`, its default). It's
Docker's own default seccomp profile, which has blocked
`io_uring_setup`/`io_uring_enter`/`io_uring_register` since Docker 25.0
([`moby/moby#46762`](https://github.com/moby/moby/pull/46762), merged
2023-11-02) — a deliberate response to io_uring's CVE history (see
[Stage 7](07-security.md)), not an oversight. A request to reverse it was
closed "not planned."

**The fix**, and exactly what this repository's own samples were built and
run against:

```bash
docker run --rm --security-opt seccomp=unconfined ubuntu:24.04 \
  bash -c "apt update -qq && apt install -y -qq liburing-dev gcc make && bash"
```

`seccomp=unconfined` disables the whole seccomp filter for that container —
fine for local development on your own trusted code, not something to do
for a container running untrusted input. See
[Stage 7](07-security.md) for the properly scoped alternative (a custom
seccomp profile that allows only the io_uring syscalls) if that distinction
matters for your use case.

## What kernel version you need, by feature

io_uring itself needs kernel **5.1+** (its debut release). Individual
operations this curriculum uses need more:

| Feature | Minimum kernel | Used in |
|---|---|---|
| io_uring itself, basic read/write | 5.1 | [Stage 1](01-mental-model.md), [Stage 2](02-liburing-basics.md) |
| `IORING_FEAT_SINGLE_MMAP` (one mmap for both rings) | 5.4 | [Stage 1](01-mental-model.md) |
| Registered files/buffers | 5.1 (files), 5.1 (buffers) | [Stage 3](03-fixed-files-and-buffers.md) |
| Multishot accept | 5.19 | [Stage 4](04-networking.md), [Stage 8](08-capstone.md) |
| Multishot recv | 6.0 | Mentioned in [Stage 4](04-networking.md) as a next step |
| `IORING_SETUP_SINGLE_ISSUER` | 6.1 | Not used directly in this curriculum; noted in resources |
| SQPOLL without special privileges | 5.13 (progressively relaxed from 5.11) | [Stage 6](06-sqpoll.md) |

(Source for the version-to-feature mapping: [kernelnewbies.org](https://kernelnewbies.org)'s
per-release pages, e.g. [Linux 5.1](https://kernelnewbies.org/Linux_5.1),
[Linux 5.19](https://kernelnewbies.org/Linux_5.19), cross-checked against the
[`axboe/liburing` wiki](https://github.com/axboe/liburing/wiki/io_uring-and-networking-in-2023).)

## Checkpoint

You're done with this stage when:

```bash
cd code/07-security
make
./seccomp_check.out
```

prints `io_uring_queue_init: SUCCESS - io_uring is usable here.` If it
doesn't, its own output tells you which of the three failure modes above
you're hitting — read that message before searching further.

Next: [Stage 1 — the mental model](01-mental-model.md).
