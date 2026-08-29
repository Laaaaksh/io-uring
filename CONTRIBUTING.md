# Contributing to io-uring

Thanks for considering a contribution. This is a curriculum plus runnable
samples, open source under the MIT license.

## Getting started

```bash
git clone https://github.com/<your-username>/io-uring.git   # your fork
cd io-uring
```

You need a Linux machine with a modern kernel (6.6+, ideally 6.11/6.12 - see
[`curriculum/00-toolchain-setup.md`](curriculum/00-toolchain-setup.md)) and
`liburing-dev` installed to build and run the samples. If you're on macOS or
Windows, or your kernel is older, Stage 0 covers running everything in a
Linux container instead - compiling and *running* io_uring code doesn't need
real hardware, just a real Linux kernel, which a container gives you for
free. This repo's own samples were built and run exactly that way; see the
root README's "How this was verified" section for the precise command.

## Contribution workflow

1. Fork the repo, clone your fork (command above).
2. Create a descriptively named branch off `main`.
3. Make focused commits.
4. If you touched anything under `code/`, prove it still builds *and runs* -
   paste the output of `make && make run` (or your own equivalent) in the
   PR, along with your kernel version (`uname -r`) and liburing version
   (`pkg-config --modversion liburing`). io_uring behavior is genuinely
   version-dependent, so this matters more here than in most repos.
5. If you touched `resources/curated-resources.md`, open every link you're
   adding or changing and confirm it resolves before submitting. Never add a
   link you haven't personally opened.
6. Open a pull request against `main`.

A PR can merge only when CI passes and review feedback is resolved.

## What contributions are useful

- Fixing a wrong claim, a stale "current" statement, or a dead link.
- A new sample that isolates one concept the way the existing ones do (see
  "Adding a sample" below) - open an issue first so scope is agreed before you
  write it.
- Corrections from testing on a kernel version or liburing version this repo
  hasn't been checked against - say which combination you used, and whether
  behavior actually differed.
- Curriculum sequencing feedback: if a stage assumes something the previous
  stage didn't actually teach, that's a real bug in a course, not a nitpick.
- Real hardware/kernel benchmark numbers for anything this repo's samples
  measure - see each sample's README for what's already measured vs. still
  asserted.

## Adding a sample

Each directory under `code/` demonstrates exactly one idea. Follow the
existing pattern:

- One or more `.c` files, one `Makefile`, one `README.md` explaining what the
  sample shows, what to look for in the output, and how it connects to the
  curriculum stage that references it.
- Comments in the code explain *why* a line matters for the concept being
  taught (why this SQE is linked to the next one, why this buffer is
  registered), not what a liburing call does structurally - link to the man
  page for that.
- The Makefile should build with a single `make` and clean with `make clean`.
- State the exact kernel version and liburing version the sample was run
  against, in the sample's own README - not only in a top-level doc.
- Prefer extending an existing stage's "prove it stuck" exercise over adding
  a new curriculum stage - new stages change the sequencing for everyone.

## Code style

- Comments explain *why*, not *what* - the reader can read C syntax, they
  need help with the parts that are non-obvious (why this op is linked to
  that one, what a short read means for `IOSQE_IO_LINK`, why a completion is
  checked before reusing a buffer).
- Match the error-checking pattern already used across samples (checking
  both the `liburing` setup calls and each CQE's `res` field) rather than
  inventing a new one.

## Reporting issues

Open a GitHub issue before starting anything larger than a typo fix, so scope
is agreed first. Use the bug report template for something broken, and the
resource suggestion template for anything about `resources/curated-resources.md`.
