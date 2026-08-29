// Fires N connections at a server all at once (via fork, so they're
// genuinely concurrent processes, not a loop pretending to be concurrent)
// and times how long the whole batch takes to get an echo back.
//
// Companion to curriculum/08-capstone.md.
//
// This is the same load applied to all three servers in this directory -
// same client, same connection count, same message. What's under test is
// entirely the server side's concurrency handling.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../common/common.h"

#define MSG "ping"

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static int one_request(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return 1;
    }
    if (write(fd, MSG, strlen(MSG)) < 0) {
        close(fd);
        return 1;
    }
    char buf[16] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    return (n == (ssize_t)strlen(MSG) && memcmp(buf, MSG, (size_t)n) == 0) ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <num_clients>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    int num_clients = atoi(argv[2]);

    double t0 = now_ms();

    for (int i = 0; i < num_clients; i++) {
        pid_t pid = fork();
        CHECK_ERRNO(pid, "fork");
        if (pid == 0) {
            _exit(one_request(port));
        }
    }

    int failures = 0;
    for (int i = 0; i < num_clients; i++) {
        int status;
        wait(&status);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            failures++;
    }

    double elapsed = now_ms() - t0;
    printf("%d clients, %d failed, %.2f ms total (%.0f connections/sec)\n",
           num_clients, failures, elapsed, num_clients / (elapsed / 1000.0));
    return failures > 0 ? 1 : 0;
}
