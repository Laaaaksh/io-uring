// The io_uring side of this capstone's three-way benchmark: the same
// server as code/04-echo-server/uring_echo.c (see that stage for the full
// walkthrough of how it works), duplicated here so this directory's
// three servers - this one, epoll_srv.c, and blocking_srv.c - build and
// run independently of each other.
//
// Companion to curriculum/08-capstone.md.
//
// The shape: submit a *multishot* accept once - it keeps producing new
// client connections without you resubmitting it after each one, unlike a
// plain accept() or a single-shot IORING_OP_ACCEPT. Each new connection
// gets a recv posted; when data arrives, echo it back with a send, then
// post another recv for that same connection. Every one of those ops goes
// through the same ring and the same completion loop - the "one thread
// serving many connections" idea epoll also gives you, but with fewer
// syscalls per event and without epoll's separate readiness-then-read
// step (see epoll_srv.c and curriculum/08-capstone.md for the side-by-side).
//
// This server exits after serving `num_connections` connections (each one
// message in, one message out, then close) - that makes it possible to
// test deterministically instead of needing to kill a long-running
// process. Pass a large number for interactive use.
//
// Verified against: Linux 6.12, liburing 2.5 (see repo root README).
#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/common.h"

#define QUEUE_DEPTH 64
#define BUF_SIZE 1024

// Every in-flight operation needs a tag telling the completion handler
// what it was and which connection it belongs to - the CQE only gives you
// user_data back, so this struct *is* that tag. Allocated per-op instead
// of reused because ops for the same connection (recv then send) can be
// in flight-adjacent, not overlapping-same-slot, in this simple server.
enum op_type { OP_ACCEPT, OP_RECV, OP_SEND };
struct conn_op {
    enum op_type type;
    int fd;
    char buf[BUF_SIZE];
};

static void add_accept(struct io_uring *ring, int listen_fd,
                        struct sockaddr_in *client_addr,
                        socklen_t *client_len) {
    struct conn_op *op = calloc(1, sizeof(*op));
    op->type = OP_ACCEPT;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    // Multishot: this one accept SQE stays "live" and produces a new CQE
    // for every incoming connection until it's explicitly canceled or the
    // ring is torn down. A single-shot accept would need re-submitting
    // after every single connection just to keep listening.
    io_uring_prep_multishot_accept(sqe, listen_fd, (struct sockaddr *)client_addr,
                                    client_len, 0);
    io_uring_sqe_set_data(sqe, op);
}

static void add_recv(struct io_uring *ring, int fd) {
    struct conn_op *op = calloc(1, sizeof(*op));
    op->type = OP_RECV;
    op->fd = fd;
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, op->buf, BUF_SIZE, 0);
    io_uring_sqe_set_data(sqe, op);
}

static void add_send(struct io_uring *ring, int fd, const char *data, int len) {
    struct conn_op *op = calloc(1, sizeof(*op));
    op->type = OP_SEND;
    op->fd = fd;
    memcpy(op->buf, data, (size_t)len);
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_send(sqe, fd, op->buf, (size_t)len, 0);
    io_uring_sqe_set_data(sqe, op);
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

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    CHECK_ERRNO(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)), "bind");
    // A large backlog matters for this capstone specifically: the
    // benchmark client fires every connection at once, and a small
    // backlog would reject connections outright before this server's
    // concurrency handling even comes into play.
    CHECK_ERRNO(listen(listen_fd, 1024), "listen");
    printf("listening on 127.0.0.1:%d, will serve %d connection(s)\n", port,
           num_connections);
    fflush(stdout);

    struct io_uring ring;
    CHECK_RC(io_uring_queue_init(QUEUE_DEPTH, &ring, 0), "io_uring_queue_init");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    add_accept(&ring, listen_fd, &client_addr, &client_len);
    CHECK_RC(io_uring_submit(&ring), "io_uring_submit (initial accept)");

    int served = 0;
    while (served < num_connections) {
        struct io_uring_cqe *cqe;
        CHECK_RC(io_uring_wait_cqe(&ring, &cqe), "io_uring_wait_cqe");
        struct conn_op *op = io_uring_cqe_get_data(cqe);
        int res = cqe->res;
        // IORING_CQE_F_MORE tells us a multishot op will keep producing
        // completions - if it's *not* set here, the kernel is telling us
        // this multishot accept stopped (e.g. an error) and we'd need to
        // resubmit it to keep accepting. This server doesn't handle that
        // resubmission case since it always serves a bounded number of
        // connections and exits, but production code must check this.
        int more = (cqe->flags & IORING_CQE_F_MORE) != 0;
        io_uring_cqe_seen(&ring, cqe);

        switch (op->type) {
        case OP_ACCEPT:
            if (res < 0) {
                fprintf(stderr, "accept failed: %s\n", strerror(-res));
            } else {
                add_recv(&ring, res);
            }
            // A multishot SQE is submitted ONCE but produces MANY
            // completions, all carrying the same user_data pointer we set
            // at submission time - freeing `op` after the first one would
            // leave every later accept completion pointing at freed
            // memory. Only free it once `more` says the kernel is done
            // with this SQE, then re-arm with a fresh op.
            if (!more) {
                free(op);
                add_accept(&ring, listen_fd, &client_addr, &client_len);
            }
            break;

        case OP_RECV:
            if (res <= 0) {
                // 0 means the peer closed the connection; negative is a
                // real error. Either way, this connection is done.
                if (res < 0)
                    fprintf(stderr, "recv failed on fd=%d: %s\n", op->fd,
                            strerror(-res));
                close(op->fd);
            } else {
                add_send(&ring, op->fd, op->buf, res);
            }
            free(op);
            break;

        case OP_SEND:
            if (res < 0) {
                fprintf(stderr, "send failed on fd=%d: %s\n", op->fd,
                        strerror(-res));
                close(op->fd);
            } else {
                // Echoed successfully - this connection's one exchange is
                // done for this sample's purposes; count it and close.
                served++;
                close(op->fd);
            }
            free(op);
            break;
        }

        CHECK_RC(io_uring_submit(&ring), "io_uring_submit (loop)");
    }

    printf("served %d connection(s), exiting\n", served);
    io_uring_queue_exit(&ring);
    close(listen_fd);
    return 0;
}
