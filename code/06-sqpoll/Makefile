CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=gnu11
LDLIBS := -luring

TARGET := sqpoll_demo.out
SRC := sqpoll_demo.c

$(TARGET): $(SRC) ../common/common.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

.PHONY: run test clean
run: $(TARGET)
	./$(TARGET)

test: run

clean:
	rm -f $(TARGET) testfile.txt
