# 03 — Fixed files and registered buffers

Registers a file and a buffer with the ring once (`io_uring_register_files`,
`io_uring_register_buffers`), then times ~20,000 reads two ways: plain fd
and buffer, versus `IOSQE_FIXED_FILE` + `io_uring_prep_read_fixed`
referencing the registered ones by index.

Companion to [`curriculum/03-fixed-files-and-buffers.md`](../../curriculum/03-fixed-files-and-buffers.md).

## Build and run

```bash
make
./fixed_vs_plain.out
```

## What was measured — read this before trusting either number

This is a genuine benchmark, and its result was **not consistent** across
repeated runs in this repository's own testing (Linux 6.12, a shared,
virtualized environment, not dedicated hardware). Four separate runs:

```
plain fd + plain buffer:    33.80 ms  (1.690 us/op)
fixed file + fixed buf:     34.73 ms  (1.736 us/op)
fixed path was NOT faster in this run.

plain fd + plain buffer:   591.66 ms  (29.583 us/op)
fixed file + fixed buf:    720.46 ms  (36.023 us/op)
fixed path was NOT faster in this run.

plain fd + plain buffer:    31.29 ms  (1.564 us/op)
fixed file + fixed buf:     29.70 ms  (1.485 us/op)
fixed path was 5.1% faster in this run.

plain fd + plain buffer:    27.25 ms  (1.362 us/op)
fixed file + fixed buf:     28.77 ms  (1.439 us/op)
fixed path was NOT faster in this run.
```

The mechanism registered files/buffers use to save work (skipping
per-operation `fget`/`fput` and buffer pinning) is real and documented —
see [`io_uring_register(2)`](https://man7.org/linux/man-pages/man2/io_uring_register.2.html).
What this repo's testing shows honestly is that **on this specific
workload** (repeated reads of one small, already page-cache-resident file)
the effect is small — often within the noise of a shared virtualized
environment, sometimes measurably faster, once not measurably different at
all, and one run (the 591ms/720ms pair) shows a large swing in *both*
numbers together, most likely a scheduling hiccup in the host environment
rather than anything about the mechanism itself.

**Don't conclude from this sample that registered files/buffers don't
matter.** Conclude that their benefit is workload-dependent, and that this
particular micro-benchmark isn't big enough or contended enough to show it
clearly and repeatably. See
[`code/05-file-copy`](../05-file-copy)'s random-read benchmark for a
workload where a related "tell the kernel more up front" technique (many
ops in flight) shows an 11–23× effect — unambiguous, repeatable, real.

## What to look for

- `io_uring_prep_read_fixed()` still takes a real buffer pointer, not
  `NULL` — the kernel checks that the address falls within the range you
  registered for that buffer index. An earlier version of this sample
  passed `NULL` and got `EFAULT` (Bad address); see the comment in
  `fixed_vs_plain.c` for the fix. This is exactly the kind of thing
  "fixed" can trick you into assuming, and exactly why it's called out
  here.
- Run `./fixed_vs_plain.out` five or six times yourself and watch the
  variance directly, rather than trusting the table above — reproducing
  the noise is more instructive than reading about it.
