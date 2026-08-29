// Diagnoses why io_uring might not be available here, and demonstrates the
// exact restriction this stage's README talks about: Docker's default
// seccomp profile blocks the io_uring syscalls outright.
//
// Companion to curriculum/07-security.md.
//
// This isn't a synthetic example - every failure mode this program
// distinguishes was hit for real while building this repository. Run it
// under a few different sandboxing configurations (see the README) and
// the exit code and message tell you which of three different things is
// actually going on, which matters because the fixes are different:
//   - ENOSYS: this kernel build doesn't have io_uring at all.
//   - EPERM from a seccomp filter: the sandbox is deliberately blocking it
//     - this is Docker's *default* behavior since Docker 25.0
//       (moby/moby#46762, merged 2023-11-02), specifically because of
//       io_uring's CVE history (see the README for citations).
//   - EPERM/ENOSYS from the io_uring_disabled sysctl: an admin explicitly
//     turned it off system-wide, at one of two levels.
//
// Verified against: Linux 6.12, liburing 2.5 (see repo root README).
#include <errno.h>
#include <liburing.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    // Read the io_uring_disabled sysctl first, since it changes how to
    // interpret a failure below. Values: 0 = enabled for everyone (the
    // default), 1 = enabled only for processes already holding
    // CAP_SYS_ADMIN, 2 = disabled entirely, even for root. Not present at
    // all on kernels built without this knob (rare on anything current).
    FILE *f = fopen("/proc/sys/kernel/io_uring_disabled", "r");
    if (f) {
        int val = -1;
        if (fscanf(f, "%d", &val) == 1) {
            const char *meaning =
                val == 0   ? "enabled for everyone (default)"
                : val == 1 ? "restricted to processes with CAP_SYS_ADMIN"
                           : "disabled entirely, even for root";
            printf("io_uring_disabled sysctl = %d (%s)\n", val, meaning);
        }
        fclose(f);
    } else {
        printf("io_uring_disabled sysctl not present on this kernel\n");
    }

    struct io_uring ring;
    int rc = io_uring_queue_init(8, &ring, 0);

    if (rc == 0) {
        printf("io_uring_queue_init: SUCCESS - io_uring is usable here.\n");
        io_uring_queue_exit(&ring);
        return 0;
    }

    int err = -rc;
    printf("io_uring_queue_init: FAILED - %s (errno %d)\n", strerror(err),
           err);

    if (err == ENOSYS) {
        printf(
            "ENOSYS means the io_uring_setup syscall doesn't exist as far "
            "as this process can tell - either a genuinely old kernel "
            "(pre-5.1), or a seccomp filter configured to return ENOSYS "
            "for a blocked syscall instead of EPERM (some sandboxes do "
            "this specifically so the caller sees 'no such syscall' "
            "rather than 'permission denied').\n");
    } else if (err == EPERM) {
        printf(
            "EPERM most likely means a seccomp filter is blocking the "
            "io_uring syscalls specifically - this is Docker's *default* "
            "seccomp profile since Docker 25.0 (see this sample's README for "
            "why). It can also mean the io_uring_disabled sysctl is set "
            "to 1 and this process lacks CAP_SYS_ADMIN. Check the sysctl "
            "value printed above to tell which.\n");
    }
    return 1;
}
