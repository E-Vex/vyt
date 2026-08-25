CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Wpedantic -std=c17 -O2
SRC     := $(wildcard src/*.c)
TARGET  := vyt

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(HOME)/.local/bin/$(TARGET)

clean:
	rm -f $(TARGET)
