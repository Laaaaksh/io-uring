CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11

TARGET := raw_read.out
SRC := raw_read.c

$(TARGET): $(SRC) ../common/common.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

.PHONY: run test clean
run: $(TARGET)
	./$(TARGET)

test: run

clean:
	rm -f $(TARGET) testfile.txt
