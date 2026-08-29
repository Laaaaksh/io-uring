// The same echo server as code/04-echo-server, built on epoll instead of
// io_uring - the comparison point for this capstone's benchmark.
//
// Companion to curriculum/08-capstone.md.
//
// Shape: one epoll instance, the listening socket and every client socket
// registered on it, non-blocking throughout. epoll_wait blocks until
// something is ready, then this loop calls accept()/read()/write() itself
// for whichever fd(s) became ready - unlike the io_uring version, where
// the read/write themselves are also queued as async operations. This is
// the "readiness" model curriculum/08-capstone.md's README puts side by
// side with io_uring's "completion" model.
//
// Verified against: Linux 6.12 (see repo root README).
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/common.h"

#define MAX_EVENTS 256
#define BUF_SIZE 1024

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    CHECK_ERRNO(fcntl(fd, F_SETFL, flags | O_NONBLOCK), "fcntl O_NONBLOCK");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <num_connections_to_serve>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int num_connections = atoi(argv[2]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERRNO(listen_fd, "socket");
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    set_nonblocking(listen_fd);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    CHECK_ERRNO(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)), "bind");
    CHECK_ERRNO(listen(listen_fd, 1024), "listen");
    printf("listening on 127.0.0.1:%d, will serve %d connection(s)\n", port,
           num_connections);
    fflush(stdout);

    int epfd = epoll_create1(0);
    CHECK_ERRNO(epfd, "epoll_create1");

    struct epoll_event ev = {.events = EPOLLIN, .data.fd = listen_fd};
    CHECK_ERRNO(epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev), "epoll_ctl add listen");

    struct epoll_event events[MAX_EVENTS];
    char buf[BUF_SIZE];
    int served = 0;

    while (served < num_connections) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        CHECK_ERRNO(n, "epoll_wait");

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // Level-triggered epoll re-reports a readable listen_fd as
                // long as the accept backlog isn't empty, so draining it
                // in a loop here (instead of one accept per wakeup) is
                // both correct and avoids extra epoll_wait round trips
                // when several connections arrive at once.
                for (;;) {
                    int client_fd = accept(listen_fd, NULL, NULL);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        fprintf(stderr, "accept failed: %s\n", strerror(errno));
                        break;
                    }
                    set_nonblocking(client_fd);
                    struct epoll_event cev = {.events = EPOLLIN, .data.fd = client_fd};
                    CHECK_ERRNO(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev),
                                "epoll_ctl add client");
                }
                continue;
            }

            // A ready client fd: read what's available, echo it back. Real
            // servers have to handle EAGAIN on both read and write (the
            // socket buffer can be full) and partial writes; this sample
            // skips that for the same reason code/04-echo-server does -
            // one small message per connection never triggers either case,
            // and curriculum/04-networking.md says so explicitly rather
            // than silently relying on it.
            ssize_t r = read(fd, buf, sizeof(buf));
            if (r <= 0) {
                if (r < 0)
                    fprintf(stderr, "read failed on fd=%d: %s\n", fd, strerror(errno));
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                continue;
            }
            ssize_t w = write(fd, buf, (size_t)r);
            if (w < 0)
                fprintf(stderr, "write failed on fd=%d: %s\n", fd, strerror(errno));

            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
            served++;
        }
    }

    printf("served %d connection(s), exiting\n", served);
    close(epfd);
    close(listen_fd);
    return 0;
}
