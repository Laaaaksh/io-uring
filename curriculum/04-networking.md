# Stage 4 — Networking

**You'll be able to:** build a single-threaded TCP server on io_uring using
multishot accept, and explain precisely how that differs from an
epoll-based server doing the same job — because you'll have built both.

**Time:** 3–4 hours.

**Build:** [`code/04-echo-server`](../code/04-echo-server).

## The idea this stage exists to teach

epoll tells you an fd is *ready* — you still call `read()`/`write()`
yourself, synchronously, once you're told. io_uring's `recv`/`send`/`accept`
operations are *themselves* queued and completed asynchronously: you get
told the result, not just that something is ready. That's a real
architectural difference, not just a different API for the same thing, and
the clearest way to actually feel it is to build the same server both ways
and read the two implementations side by side — which is exactly what
[`code/08-capstone`](../code/08-capstone) later benchmarks.

Multishot accept is the other half of this stage's point: a single
`IORING_OP_ACCEPT` submitted with the multishot flag keeps producing a new
completion for every incoming connection until you cancel it, instead of
needing to be resubmitted after every single connection like a plain
accept would.

## Read or watch, in this order

1. **[`axboe/liburing` wiki — "io_uring and networking in 2023"](https://github.com/axboe/liburing/wiki/io_uring-and-networking-in-2023)**
   — the best single networking-specific writeup, from the maintainer's own
   wiki. Covers batching, multishot ops, and provided buffers. Dated
   2023-02-14 and covers up to kernel 6.1 — current on the concepts this
   stage uses, though `IORING_OP_SEND_ZC` (zero-copy send, mentioned below)
   matured somewhat after this was written.
2. **[`io_uring_prep_multishot_accept(3)`](https://man7.org/linux/man-pages/man3/io_uring_prep_multishot_accept.3.html)**
   and **[`io_uring_prep_recv(3)`](https://man7.org/linux/man-pages/man3/io_uring_prep_recv.3.html)**
   — the two ops [`code/04-echo-server`](../code/04-echo-server) is built
   from.

## Do

Read [`code/04-echo-server/uring_echo.c`](../code/04-echo-server/uring_echo.c)
first, in full, before running anything — it's short, and the shape (one
multishot accept, a recv posted per new connection, a send posted per
completed recv, closing and repeating) is the whole stage in one file. Then
build and run its test:

```bash
cd code/04-echo-server
make test
```

This starts the server, drives three real client connections against it
over a real TCP socket on `127.0.0.1`, and checks each one gets its own
message echoed back correctly.

Then, deliberately break the use-after-free this sample's own development
process hit (see
[`resources/common-pitfalls.md`](../resources/common-pitfalls.md) for the
full story): move the `free(op)` call in the `OP_ACCEPT` case so it runs
unconditionally instead of only when `!more`. Rebuild, run `make test` with
more than one connection, and watch it crash or print garbage — this is
the single most instructive bug to trigger on purpose in this entire
curriculum, because "multishot ops share one `user_data` pointer across
many completions" is easy to read and easy to still get wrong once you're
writing your own code.

## Checkpoint

- Why does this server only call `add_accept()` again when `cqe->flags &
  IORING_CQE_F_MORE` is *unset* — what would go wrong if it re-armed on
  every completion instead?
- If you wanted this server to handle a connection sending more than
  `BUF_SIZE` bytes at once, what would have to change? (The sample doesn't
  handle this — say why not, and what real code would need instead.)
- In your own words: what does an epoll-based version of this same server
  have to do differently, structurally, not just in which functions it
  calls?

Next: [Stage 5 — storage](05-storage.md).
