CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS_URING := -luring

NUM_CLIENTS := 300

all: uring_srv.out epoll_srv.out blocking_srv.out bench_client.out

uring_srv.out: uring_srv.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ uring_srv.c $(LDLIBS_URING)

epoll_srv.out: epoll_srv.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ epoll_srv.c

blocking_srv.out: blocking_srv.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ blocking_srv.c

bench_client.out: bench_client.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ bench_client.c

.PHONY: bench test clean

# The actual capstone: same $(NUM_CLIENTS)-connection concurrent load
# against all three servers, one after another, each on its own port.
# Client failure (a dropped/mismatched connection) fails the whole target -
# `wait $$SPID` alone wouldn't propagate that, since it's the last command
# in each line, so the client's exit status is captured explicitly.
bench: all
	@echo "=== io_uring server ==="
	./uring_srv.out 28001 $(NUM_CLIENTS) & \
	SPID=$$!; sleep 0.3; ./bench_client.out 28001 $(NUM_CLIENTS); CSTATUS=$$?; wait $$SPID; exit $$CSTATUS
	@echo ""
	@echo "=== epoll server ==="
	./epoll_srv.out 28002 $(NUM_CLIENTS) & \
	SPID=$$!; sleep 0.3; ./bench_client.out 28002 $(NUM_CLIENTS); CSTATUS=$$?; wait $$SPID; exit $$CSTATUS
	@echo ""
	@echo "=== blocking (one-at-a-time) server ==="
	./blocking_srv.out 28003 $(NUM_CLIENTS) & \
	SPID=$$!; sleep 0.3; ./bench_client.out 28003 $(NUM_CLIENTS); CSTATUS=$$?; wait $$SPID; exit $$CSTATUS

# Deterministic for CI: same three-way run, but fails the build if any
# server drops or mishandles a connection - the perf numbers are printed
# either way, but CI is checking correctness under concurrency, not speed.
test: bench

clean:
	rm -f uring_srv.out epoll_srv.out blocking_srv.out bench_client.out
