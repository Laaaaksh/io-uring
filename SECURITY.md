# Security Policy

io-uring is a set of educational docs and small, self-contained C sample
programs. There's no running service, no server component that's meant to
face untrusted input, and nothing here is a library other code depends on -
so the realistic attack surface is narrow.

One clarification specific to this repo: io_uring itself has a real, well
documented security history in the Linux kernel (see
[`curriculum/07-security.md`](curriculum/07-security.md) and
[`resources/curated-resources.md`](resources/curated-resources.md)'s
Security section). That history is course content, not a vulnerability in
this repository - don't report "io_uring has had kernel CVEs" as an issue
here, it's the whole point of that stage.

## What belongs in a report

Worth reporting privately:

- A sample program that does something unsafe with untrusted input (none
  are designed to take untrusted input over the network from anyone but
  `localhost` in a lab you run yourself, so this would itself be a bug).
- A build script or Makefile that fetches and executes something from the
  network without saying so.
- The `code/07-security` seccomp/capability demo doing something it
  shouldn't outside its documented, contained scope.

Not a security issue, just a normal bug report (open a public issue
instead):

- A sample that compiles but produces the wrong output.
- A memory bug inside a sample that only ever operates on data the sample
  itself generates.
- A dead or incorrect link in the curated resources.
- A claim about an io_uring kernel CVE that's outdated or wrong - that's a
  correctness bug in the curriculum, not a report about this repo.

## Reporting a vulnerability

Use GitHub's private vulnerability reporting:

> https://github.com/Laaaaksh/io-uring/security/advisories/new

That reaches the maintainer privately so any real issue can be fixed before
it's discussed in public.

## Credits

Reporters who wish to be credited may say so in the private report;
otherwise reports are handled without attribution.
