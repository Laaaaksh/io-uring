# 04 — Echo server

A single-threaded TCP echo server built entirely on io_uring: one ring,
multishot accept, a recv/send pair per connection, no thread per
connection.

Companion to [`curriculum/04-networking.md`](../../curriculum/04-networking.md).

## Build and run

```bash
make          # builds uring_echo.out and client.out
make test     # starts the server, drives 3 real client connections, checks each echo
```

Interactively:

```bash
./uring_echo.out 8080 1000   # port, connections to serve before exiting
# in another terminal:
./client.out 8080 "hello"
```

## What was measured

`make test` is a correctness check, not a benchmark (the three-way
performance comparison against epoll and blocking I/O lives in
[`code/08-capstone`](../08-capstone)). Real output from this repository's
own testing:

```
$ make test
listening on 127.0.0.1:18888, will serve 3 connection(s)
ok: echoed "hello" correctly
ok: echoed "second message" correctly
ok: echoed "third!" correctly
served 3 connection(s), exiting
```

## The bug this sample's own development hit — worth reading even though it's fixed

An earlier version of `uring_echo.c` freed the `OP_ACCEPT` completion's
`user_data` struct unconditionally, the first time any accept completion
arrived. That's correct for a *single-shot* op, and wrong for a
**multishot** one: a multishot accept SQE is submitted once but produces a
new completion for every incoming connection, and every one of those
completions carries the *same* `user_data` pointer set at submission time.
Freeing it after the first connection left every subsequent accept
completion pointing at freed memory — a real use-after-free that showed up
as a segfault with a garbled `op->type` field when tested with more than
one connection. The fix (still in the code, with the full explanation in a
comment): only free the tag once `cqe->flags & IORING_CQE_F_MORE` is
*unset*, meaning the kernel is actually done with that SQE.

## What to look for

- `IORING_CQE_F_MORE` in the completion-handling `switch` — this is the
  flag that tells you whether a multishot op will keep producing
  completions. Every multishot user has to check it; there's no other way
  to know a multishot SQE has stopped.
- This server closes each connection after one recv/send round trip — a
  deliberate simplification so `make test` can be deterministic (the
  server exits after serving a fixed connection count). A real echo server
  would loop recv→send→recv on the same connection until the peer closes
  it; the comment in the source says so rather than silently limiting
  scope.
- `client.c` is a plain blocking client, on purpose — the point under test
  is the *server's* concurrency model, not the client's.
