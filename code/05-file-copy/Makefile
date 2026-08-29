CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS := -luring

TEST_FILE := testfile.bin
TEST_SIZE_MB := 64

all: io_uring_cp.out plain_cp.out random_read_bench.out

io_uring_cp.out: io_uring_cp.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ io_uring_cp.c $(LDLIBS)

plain_cp.out: plain_cp.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ plain_cp.c

random_read_bench.out: random_read_bench.c ../common/common.h
	$(CC) $(CFLAGS) -o $@ random_read_bench.c $(LDLIBS)

.PHONY: run test clean
# Interactive use: copy a real file with io_uring_cp.
# usage: make run SRC=/path/to/file DST=/path/to/copy
run: io_uring_cp.out
	./io_uring_cp.out $(SRC) $(DST)

# Deterministic for CI: generate a file, copy it both ways, verify the
# copies are byte-identical to the source, and print both tools' timing.
test: all
	dd if=/dev/urandom of=$(TEST_FILE) bs=1M count=$(TEST_SIZE_MB) status=none
	./io_uring_cp.out $(TEST_FILE) $(TEST_FILE).uring.copy
	./plain_cp.out $(TEST_FILE) $(TEST_FILE).plain.copy
	cmp $(TEST_FILE) $(TEST_FILE).uring.copy
	cmp $(TEST_FILE) $(TEST_FILE).plain.copy
	@echo "both copies are byte-identical to the source"
	@echo ""
	./random_read_bench.out $(TEST_FILE)

clean:
	rm -f io_uring_cp.out plain_cp.out random_read_bench.out $(TEST_FILE) $(TEST_FILE).uring.copy $(TEST_FILE).plain.copy
