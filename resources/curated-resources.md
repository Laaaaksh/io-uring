# Curated resources

Every entry here was actually opened and checked at the time of writing
(August 2026) — not recalled from memory. Dates are the resource's own
stated publish/update date, or a GitHub push/release date fetched directly,
where one exists. Where something has aged, that is said plainly, along
with what to read instead or alongside it. Curriculum stages link the
specific entries relevant to them; this page is the full, browsable list
with the reasoning behind each recommendation.

If a link here breaks, or you find something better, please open an issue
using the "Resource suggestion" template — see
[`CONTRIBUTING.md`](../CONTRIBUTING.md).

## Official and canonical

| Resource | What it covers | Current as of | Verdict |
|---|---|---|---|
| [`axboe/liburing`](https://github.com/axboe/liburing) | The library every sample in this repo uses — setup/teardown helpers, a simplified prep/submit/wait interface, and 228 man pages under `man/` covering every op and every liburing function | Pushed 2026-08-28; latest tagged release `liburing-2.15` (2026-06-29) | **The single most important resource here.** Not tied to a specific kernel — newer liburing works on older kernels, falling back gracefully. Its `examples/` directory is worth reading end to end after Stage 2. Note: Ubuntu 24.04's `apt` package is liburing **2.5** (see [Stage 0](../curriculum/00-toolchain-setup.md)) — meaningfully older than upstream 2.15; this repo was built and verified against 2.5 specifically, and says so in every sample. |
| [io_uring(7)](https://man7.org/linux/man-pages/man7/io_uring.7.html), [io_uring_setup(2)](https://man7.org/linux/man-pages/man2/io_uring_setup.2.html), [io_uring_enter(2)](https://man7.org/linux/man-pages/man2/io_uring_enter.2.html), [io_uring_register(2)](https://man7.org/linux/man-pages/man2/io_uring_register.2.html) | The three raw syscalls Stage 1 uses directly, and the concept overview | HTML rendered 2026-05-30, upstream commit 2026-05-18 | Current and authoritative — these are shipped from the liburing repo's own `man/` tree, not a separate stale copy. Read `io_uring(7)` first; it has a complete example program. |
| [`axboe/liburing` wiki — "io_uring and networking in 2023"](https://github.com/axboe/liburing/wiki/io_uring-and-networking-in-2023) | Batching, multishot ops, provided buffers, `IORING_SETUP_DEFER_TASKRUN`/`SINGLE_ISSUER`, ring messages | Authored 2023-02-14, covers up to kernel 6.1 | **The best single networking-specific writeup**, from the maintainer's own wiki. [Stage 4](../curriculum/04-networking.md) builds a multishot-accept server per this doc's recommendations; it predates zero-copy send/receive maturing further and `IORING_OP_SEND_ZC` landing, which [Stage 4](../curriculum/04-networking.md) covers separately. |
| ["Efficient IO with io_uring"](https://kernel.dk/io_uring.pdf) — Jens Axboe | The original design rationale: why Linux `aio` fell short, the three design goals (easy to use, extendable, efficient), and the SQ/CQ ring model | Undated PDF; written around io_uring's 5.1 debut (2019) based on content | The primary source for *why* io_uring exists, in the designer's own words. Concepts hold up completely — nothing here has gone stale, since it's about motivation and design goals rather than current API surface. Read after [Stage 1](../curriculum/01-mental-model.md), not before — it lands better once you've felt raw syscalls yourself. |

## History and design evolution (LWN.net)

| Resource | What it covers | Date | Verdict |
|---|---|---|---|
| ["Ringing in a new asynchronous I/O API"](https://lwn.net/Articles/776703/) — Jonathan Corbet | The first public writeup of io_uring's proposed design, before it shipped | 2019-01-15 | Historical, and a good short read for how the ring/SQE/CQE model was originally pitched. Not a tutorial. |
| ["The rapid growth of io_uring"](https://lwn.net/Articles/810414/) | The opcode set as of kernel 5.5, and the growing-pains concerns (8-bit opcode field, chaining data between ops) raised at the time | 2020-01-24 | Good context for how fast the opcode surface has grown — most of the concerns raised here were later addressed; read as history, not a current API reference. |
| ["Axboe: What's new with io_uring in 6.10"](https://lwn.net/Articles/974341/) | "Bundles" — multiple buffers in a single operation, for both send and receive | 2024-05-20 | One snapshot of the ongoing "what shipped this kernel" pattern LWN has followed for years — search LWN's io_uring tag for the release you care about rather than treating any one of these as current. |
| ["Task-level io_uring restrictions"](https://lwn.net/Articles/1054225/) — Jonathan Corbet | Jens Axboe's in-progress work to let `seccomp`-style filtering apply to io_uring operations at the task level (`IORING_REGISTER_RESTRICTIONS_TASK`), addressing exactly the "io_uring bypasses seccomp" gap this repo's [security stage](../curriculum/07-security.md) demonstrates hands-on | 2026-01-19 | **The most current, most relevant piece here for the security chapter.** As of this writing the mechanism is still evolving (five patch revisions, moving from eBPF to classic BPF for unprivileged use) — not yet in a released kernel. Worth checking back on. |

## Security

| Resource | What it covers | Date | Verdict |
|---|---|---|---|
| ["Learnings from kCTF VRP's 42 Linux kernel exploits submissions"](https://security.googleblog.com/2023/06/learnings-from-kctf-vrp-42-linux-kernel-exploits-submissions.html) — Google Online Security Blog | io_uring accounted for 60% of Google's kernel-exploit VRP payouts (~$1M); Google's decision to disable io_uring on production servers and restrict it via SELinux on Android | 2023-06 | **Read this before writing anything that touches untrusted input near io_uring.** The companion [openwall oss-security mailing list post](https://www.openwall.com/lists/oss-security/2023/06/17/2) (2023-06-16/17) carries the same content in the original disclosure thread. |
| [CVE-2026-43174](https://www.sentinelone.com/vulnerability-database/cve-2026-43174/) | A use-after-free in io_uring's zero-copy-receive (`zcrx`) subsystem — closing a queue doesn't guarantee its page pools are torn down immediately, so a page pool can outlive its parent context | Published 2026-05-06 | Cited here specifically because it's recent, it's in a feature this repo's [Stage 4](../curriculum/04-networking.md) discusses (zero-copy networking), and it shows the CVE flow is still live in 2026, not just a 2022–2023 phenomenon. |
| [`moby/moby#46762`](https://github.com/moby/moby/pull/46762) — "seccomp: block io_uring_* syscalls in default profile" | The exact PR that made Docker's default seccomp profile block `io_uring_setup`/`io_uring_enter`/`io_uring_register` | Merged 2023-11-02 (shipped in Docker 25.0) | This is the change [`code/07-security`](../code/07-security) demonstrates hands-on. [`moby/moby#47532`](https://github.com/moby/moby/issues/47532), a request to unblock it, was closed as "not planned" — a deliberate, standing security stance, not an oversight. |

## Tutorials and courses

| Resource | What it covers | Date | Verdict |
|---|---|---|---|
| ["Lord of the io_uring"](https://unixism.net/loti/) (unixism.net) — Shuveb Hussain | 5 progressive examples: sync `cat`, raw io_uring, liburing, batched `cp`, a toy static-file webserver | Site footer: "©2020, Shuveb Hussain." Companion repos [`shuveb/loti`](https://github.com/shuveb/loti) (108★) and [`shuveb/io_uring-by-example`](https://github.com/shuveb/io_uring-by-example) (433★) last pushed 2024-08-02 and 2024-10-17 respectively — over a year stale as of this writing, despite this research's source report describing them as recently active | **Aged, and the most commonly recommended io_uring tutorial regardless.** Good for the sync-vs-async framing in its first two examples; stops at kernel 5.5-era liburing, before multishot ops, zero-copy send/receive, registered ring-mapped buffers, or SQPOLL tuning existed. This repo picks up from exactly where it stops. |
| [`noteflakes/awesome-io_uring`](https://github.com/noteflakes/awesome-io_uring) | An unordered link list of io_uring resources | 84★, pushed 2025-02-10 | A link dump, not a sequenced path — the exact shape this repo exists to not be. Useful as a secondary cross-check for finding a resource this page missed, not as a starting point. |
| No dedicated book | — | Checked O'Reilly, Manning, and No Starch Press catalogs directly, August 2026 | **None found.** io_uring has no book-length treatment from a major technical publisher as of this writing — everything durable is man pages, the liburing wiki, LWN, and blog posts of varying vintage. This is itself part of the gap this repository exists to close. |

## Production usage and current adoption

| Resource | What it covers | Date | Verdict |
|---|---|---|---|
| [PostgreSQL 18 release announcement](https://www.postgresql.org/about/news/postgresql-18-released-3142/) | The new `io_method=io_uring` asynchronous I/O backend — up to 3× throughput on sequential scans, bitmap heap scans, and vacuum; configured via the [`io_method`](https://www.postgresql.org/docs/18/runtime-config-resource.html#GUC-IO-METHOD) GUC | Released 2025-09-25 | The clearest "this is load-bearing production infrastructure now" evidence available. `io_method` defaults to `sync`, not `io_uring` — opting in is a deliberate operator decision, worth knowing before assuming any given Postgres 18 install uses it. |
| [`bytedance/monoio`](https://github.com/bytedance/monoio) | A thread-per-core, io_uring-native async Rust runtime built by ByteDance | 5,082★, pushed 2026-07-20 | Actively developed, real production use at scale. If you're evaluating whether to reach for io_uring in Rust directly rather than through Tokio's epoll-based default, this is the actively maintained option — see [Stage 8](../curriculum/08-capstone.md)'s "where this sits now." |
| [`DataDog/glommio`](https://github.com/DataDog/glommio) | A cooperative thread-per-core Rust runtime built specifically around io_uring, aimed at database/proxy/broker-shaped workloads | 3,642★, pushed 2026-04-22 | Actively maintained, narrower audience than Tokio by design (its own docs recommend it specifically for that workload shape, not as a Tokio replacement generally). |
| [`tokio-rs/tokio-uring`](https://github.com/tokio-rs/tokio-uring) | An io_uring backend for Tokio | 1,492★, pushed 2025-07-07 | **Stalled.** Over a year without a push as of this writing, changelog inactive since around 2022, many long-open issues. Mainline Tokio remains epoll-based; don't build new work assuming `tokio-uring` is a maintained path — see the two entries above for what's actually active in the Rust ecosystem instead. |

## Suggested reading order

This is denser than the curriculum's per-stage lists — use those first; come
back here for the full picture or a next step beyond what a stage asks for.

1. `io_uring(7)` for the concept overview, then Jens Axboe's "Efficient IO
   with io_uring" for the *why*.
2. "Lord of the io_uring" for its first two examples (sync vs. raw
   io_uring), knowing it stops well short of current API surface.
3. The `axboe/liburing` wiki's "io_uring and networking in 2023" once you
   reach [Stage 4](../curriculum/04-networking.md).
4. The Google security blog post and `moby/moby#46762` before
   [Stage 7](../curriculum/07-security.md) — read them, then run
   `code/07-security` yourself.
5. The PostgreSQL 18 announcement and the monoio/glommio/tokio-uring rows
   above for [Stage 8](../curriculum/08-capstone.md)'s "where this sits
   now."
6. LWN's io_uring tag (search LWN directly) for whatever kernel release
   you're currently running, since this list can't stay current with every
   release on its own.

## What's covered in the curriculum stages instead

Kernel-version-to-feature mapping (what shipped in 5.1, 5.19, 6.0, 6.1, and
so on) is covered with citations to [kernelnewbies.org](https://kernelnewbies.org)
directly in [`curriculum/00-toolchain-setup.md`](../curriculum/00-toolchain-setup.md)
rather than duplicated here.
