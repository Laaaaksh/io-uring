<div align="center">

# io-uring

**A sequenced path from "I've heard of io_uring" to "I can explain when it
actually helps, and prove it with my own numbers" — with runnable code,
actually run, at every step.**

[![CI](https://github.com/Laaaaksh/io-uring/actions/workflows/ci.yml/badge.svg)](https://github.com/Laaaaksh/io-uring/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-purple.svg)](LICENSE)
[![Verified against Linux 6.12](https://img.shields.io/badge/verified%20against-Linux%206.12-informational?logo=linux&logoColor=white)](code/README.md#how-this-was-verified)

**[Curriculum](curriculum/README.md) • [Code samples](code/README.md) • [Resources](resources/curated-resources.md) • [Common pitfalls](resources/common-pitfalls.md) • [Contributing](CONTRIBUTING.md) • [License](LICENSE)**

</div>

## What this is

io_uring has no shortage of material — man pages, a decade of blog posts,
one very good but 2020-era tutorial, LWN articles for nearly every kernel
release. What it lacks is a **path**: something that sequences that
material, tells you what's aged, and makes you actually run code instead
of reading about it. The most commonly recommended tutorial ("Lord of the
io_uring") stops at kernel-5.5-era liburing — before multishot operations,
zero-copy send/receive, or registered ring-mapped buffers existed. This
repo picks up from there.

This repo is nine sequenced stages, each with:

- **What you'll be able to do** at the end of it, stated concretely.
- **What to read**, in order — a handful of things, not a pile, with every
  claim cited and dated in
  [`resources/curated-resources.md`](resources/curated-resources.md).
- **A small, complete, commented C program to build and run**, in
  [`code/`](code) — raw syscalls with no library, liburing basics with a
  linked-chain failure you trigger on purpose, registered files and
  buffers, a real multishot-accept TCP server benchmarked against epoll
  *and* plain blocking I/O, a storage benchmark that shows io_uring winning
  by 11–23× on random reads and **not** winning at all on a sequential
  copy, SQPOLL measured honestly (sometimes a wash, once clearly slower),
  and a hands-on demonstration of exactly why Docker blocks io_uring by
  default.
- **Checkpoint questions** to answer before moving on.

Start at [`curriculum/README.md`](curriculum/README.md).

## What this doesn't cover

This stops at single-machine, single-process io_uring over files and TCP
sockets. It does not cover NVMe passthrough (`io_uring_cmd`), eBPF
integration, kernel-internals development, or language-runtime-specific
bindings beyond raw C (Rust's `tokio-uring`/`glommio`/`monoio`, Go
bindings). [Stage 8](curriculum/08-capstone.md) names the actively
maintained options in that space and where to go next; it doesn't teach
them. Full detail on every boundary in
[`curriculum/README.md`](curriculum/README.md#where-this-curriculum-stops).

## How this was verified — and why the numbers here aren't always flattering

**Unlike a lot of technologies in this position, io_uring doesn't need
exotic hardware to verify honestly — just a real, modern Linux kernel.**
Every sample in this repository was built *and actually run* (not just
compile-checked) against:

```bash
docker run --rm -it --security-opt seccomp=unconfined ubuntu:24.04
# Linux 6.12.76-linuxkit, liburing 2.5-1build1, gcc 13.3.0
```

`--security-opt seccomp=unconfined` is necessary and is itself part of the
lesson here — Docker's *default* seccomp profile has blocked io_uring's
syscalls since Docker 25.0 (2023-11-02), and [Stage 7](curriculum/07-security.md)
demonstrates that, live, as a real diagnostic exercise, not a footnote.

Every benchmark number in this repository is a **real measurement from
that environment, run multiple times**, not an asserted claim — and some
of them don't say what you'd expect:

- Random-read storage I/O: io_uring measured **11–23× faster** than plain
  `pread()`, consistently. ([`code/05-file-copy`](code/05-file-copy))
- Sequential file copy on the *same* file: plain `read()`/`write()` was
  faster than io_uring in **every** run tested.
  ([`code/05-file-copy`](code/05-file-copy))
- A three-way TCP echo-server benchmark: io_uring was, if anything, the
  **slowest** of io_uring/epoll/blocking-I/O across six repeated runs on
  localhost. ([`code/08-capstone`](code/08-capstone))
- SQPOLL vs. a plain ring: roughly a wash across repeated runs — sometimes
  marginally ahead, once clearly behind.
  ([`code/06-sqpoll`](code/06-sqpoll))

None of this means io_uring doesn't matter — PostgreSQL 18 shipped it as a
production async-I/O backend reporting up to 3× gains
([Stage 8](curriculum/08-capstone.md)). It means "io_uring is faster" is
conditional on your workload, not automatic, and this repository would
rather show you a real measured case of each than assert one. Full detail,
including CI's own run of every sample on GitHub's Linux runners, in
[`code/README.md`](code/README.md#how-this-was-verified).

## Repository layout

```
curriculum/   9 sequenced stages - the path itself
code/         8 runnable, commented samples (plus comparison baselines), one per concept
resources/    honest, dated curation of everything external cited above
```

## Contributing

Contributions are welcome — a wrong claim, a stale "current" statement, a
dead link, real numbers from your own kernel/hardware to report alongside
this repo's own. See [CONTRIBUTING.md](CONTRIBUTING.md). Please read
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) first.

## Security

Found a security issue in this repository's own code? See
[SECURITY.md](SECURITY.md) — please don't open a public issue for it. Found
a real io_uring kernel vulnerability? That goes to the [Linux kernel
security team](https://www.kernel.org/doc/html/latest/process/security-bugs.html),
not here — see [Stage 7](curriculum/07-security.md) for why this
distinction gets its own curriculum stage.

## License

MIT — see [LICENSE](LICENSE).
