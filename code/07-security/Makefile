CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS := -luring

TARGET := seccomp_check.out
SRC := seccomp_check.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

.PHONY: run test clean
run: $(TARGET)
	./$(TARGET)

# CI runs on GitHub's ubuntu-latest, which is a real VM (not a
# seccomp-restricted container), so io_uring is expected to work there -
# this asserts that. The interesting *blocked* case (see the README) is
# something to run yourself with Docker; it's not something CI can
# meaningfully assert either way without adding a Docker-in-Docker
# dependency for one demo.
test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
