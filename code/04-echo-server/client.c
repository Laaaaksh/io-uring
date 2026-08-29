// A plain blocking TCP client - no io_uring here on purpose. Its only job
// is to send one message to code/04-echo-server's server, check the same
// bytes come back, and report success/failure with an exit code, so
// `make test` has something deterministic to drive.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/common.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <message>\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    const char *msg = argv[2];

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERRNO(fd, "socket");

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    CHECK_ERRNO(connect(fd, (struct sockaddr *)&addr, sizeof(addr)), "connect");

    CHECK_ERRNO(write(fd, msg, strlen(msg)), "write");

    char buf[1024] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    CHECK_ERRNO(n, "read");
    buf[n] = '\0';

    close(fd);

    if (strcmp(buf, msg) != 0) {
        fprintf(stderr, "mismatch: sent \"%s\", got back \"%s\"\n", msg, buf);
        return 1;
    }
    printf("ok: echoed \"%s\" correctly\n", buf);
    return 0;
}
