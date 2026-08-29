// Shared error-checking macros used by every sample in this repo.
#ifndef IOURING_COMMON_H
#define IOURING_COMMON_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// liburing calls, and the raw io_uring_setup/enter/register syscalls used
// in code/01-raw-syscall, return -errno directly on failure rather than
// returning -1 and setting errno. Use this after those.
#define CHECK_RC(rc, what)                                                   \
    do {                                                                     \
        long _rc = (rc);                                                     \
        if (_rc < 0) {                                                       \
            fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__,    \
                    (what), strerror((int)-_rc));                            \
            exit(1);                                                        \
        }                                                                    \
    } while (0)

// Ordinary libc/syscall wrappers (open, mmap, socket, ...) return -1 and
// set errno. Use this after those instead.
#define CHECK_ERRNO(rc, what)                                                \
    do {                                                                     \
        long _rc = (long)(rc);                                               \
        if (_rc < 0) {                                                       \
            fprintf(stderr, "%s:%d: %s failed: %s\n", __FILE__, __LINE__,    \
                    (what), strerror(errno));                                \
            exit(1);                                                        \
        }                                                                    \
    } while (0)

#endif
