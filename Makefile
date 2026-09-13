CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Wpedantic -std=c17 -O2
SRC      := $(wildcard src/*.c)
TARGET   := vyt

TEST_SRC := tests/test_cli.c src/cli.c
TEST_BIN := tests/test_cli

# Colors
BOLD   := \033[1m
CYAN   := \033[36m
GREEN  := \033[32m
YELLOW := \033[33m
RESET  := \033[0m

.PHONY: all clean install test banner

all: banner $(TARGET)
	@printf "$(GREEN)$(BOLD)✓ Build complete -> ./$(TARGET)$(RESET)\n"

banner:
	@printf "$(CYAN)$(BOLD)"
	@printf "          _   \n"
	@printf "__ ___  _| |_ \n"
	@printf "\\ V / || |  _|\n"
	@printf " \\_/ \\_, |\\__|\n"
	@printf "     |__/     \n"
	@printf "$(RESET)"
	@printf "$(YELLOW)  building...$(RESET)\n\n"

$(TARGET): $(SRC)
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(SRC)"
	@$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

test: banner $(TEST_BIN)
	@printf "$(YELLOW)  running tests...$(RESET)\n"
	@./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC)
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_SRC)"
	@$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_BIN)

install: $(TARGET)
	@printf "$(GREEN)  installing vyt -> $(HOME)/.local/bin/$(TARGET)$(RESET)\n"
	@install -Dm755 $(TARGET) $(HOME)/.local/bin/$(TARGET)
	@install -Dm644 man/vyt.1 $(HOME)/.local/share/man/man1/vyt.1

clean:
	@printf "$(YELLOW)  cleaning...$(RESET)\n"
	@rm -f $(TARGET) $(TEST_BIN)
