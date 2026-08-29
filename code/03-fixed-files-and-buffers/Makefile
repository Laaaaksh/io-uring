CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS := -luring

TARGET := fixed_vs_plain.out
SRC := fixed_vs_plain.c

$(TARGET): $(SRC) ../common/common.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

.PHONY: run test clean
run: $(TARGET)
	./$(TARGET)

test: run

clean:
	rm -f $(TARGET) testfile.txt
