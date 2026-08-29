CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS := -luring

PORT := 18888

all: uring_echo.out client.out

uring_echo.out: uring_echo.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ uring_echo.c $(LDLIBS)

client.out: client.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ client.c

.PHONY: run test clean
# Interactive use: serves up to 1000 connections, Ctrl-C to stop.
run: uring_echo.out
	./uring_echo.out $(PORT) 1000

# Deterministic for CI: server serves exactly 3 connections then exits on
# its own; three clients connect one after another and check their echo.
test: uring_echo.out client.out
	./uring_echo.out $(PORT) 3 & \
	SERVER_PID=$$!; \
	sleep 0.3; \
	STATUS=0; \
	for msg in "hello" "second message" "third!"; do \
		./client.out $(PORT) "$$msg" || STATUS=1; \
	done; \
	wait $$SERVER_PID || STATUS=1; \
	exit $$STATUS

clean:
	rm -f uring_echo.out client.out
