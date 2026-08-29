// The baseline for this capstone's benchmark: a plain synchronous server,
// one connection at a time, nothing concurrent about it. accept(), read(),
// write(), close(), repeat. No epoll, no io_uring, no threads.
//
// Companion to curriculum/08-capstone.md.
//
// This is what "handle many connections" looks like with zero
// multiplexing - while this server is blocked in read() on connection A,
// connection B is sitting in the kernel's accept backlog, untouched, no
// matter how idle this process's CPU is. Whether that matters depends
// entirely on how many connections actually show up concurrently - see
// the README for what this measurably costs at the client counts this
// repo's benchmark uses.
//
// Verified against: Linux 6.12 (see repo root README).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/common.h"

#define BUF_SIZE 1024

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

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    CHECK_ERRNO(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)), "bind");
    // A backlog this large matters here specifically: the benchmark client
    // fires every connection at once, and with a small backlog many of
    // those connects would be refused outright instead of queuing - that
    // would make this server look artificially even worse, for a reason
    // that has nothing to do with the one-at-a-time handling this sample
    // is supposed to demonstrate.
    CHECK_ERRNO(listen(listen_fd, 1024), "listen");
    printf("listening on 127.0.0.1:%d, will serve %d connection(s)\n", port,
           num_connections);
    fflush(stdout);

    char buf[BUF_SIZE];
    int served = 0;
    while (served < num_connections) {
        int client_fd = accept(listen_fd, NULL, NULL);
        CHECK_ERRNO(client_fd, "accept");

        ssize_t r = read(client_fd, buf, sizeof(buf));
        if (r > 0) {
            ssize_t w = write(client_fd, buf, (size_t)r);
            if (w < 0)
                fprintf(stderr, "write failed: %s\n", strerror(errno));
        } else if (r < 0) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
        }

        close(client_fd);
        served++;
    }

    printf("served %d connection(s), exiting\n", served);
    close(listen_fd);
    return 0;
}
