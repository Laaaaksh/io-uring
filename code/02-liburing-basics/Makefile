CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS := -luring

TARGET := liburing_basics.out
SRC := liburing_basics.c

$(TARGET): $(SRC) ../common/common.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

.PHONY: run test clean
run: $(TARGET)
	./$(TARGET)

test: run

clean:
	rm -f $(TARGET) testfile.txt
