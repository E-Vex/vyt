CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Wpedantic -std=c17 -O2
SRC      := $(wildcard src/*.c)
TARGET   := vyt

TEST_SRC := tests/test_cli.c src/cli.c
TEST_BIN := tests/test_cli

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_BIN)

install: $(TARGET)
	install -Dm755 $(TARGET) $(HOME)/.local/bin/$(TARGET)
clean:
	rm -f $(TARGET) $(TEST_BIN)
