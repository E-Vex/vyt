CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Wpedantic -std=c17 -O2
SRC      := $(wildcard src/*.c)
TARGET   := vyt

# Each test binary links only the source files it actually exercises.
# This keeps compile times low and surfaces missing dependencies early.
TEST_CLI_SRC  := tests/test_cli.c src/cli.c
TEST_CLI_BIN  := tests/test_cli
TEST_PL_SRC   := tests/test_playlist.c src/playlist.c
TEST_PL_BIN   := tests/test_playlist
TEST_BINS     := $(TEST_CLI_BIN) $(TEST_PL_BIN)

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

test: banner $(TEST_BINS)
	@printf "$(YELLOW)  running tests...$(RESET)\n"
	@for bin in $(TEST_BINS); do \
		printf "$(CYAN)  RUN$(RESET) %s\n" "$$bin"; \
		./$$bin || exit 1; \
	done

$(TEST_CLI_BIN): $(TEST_CLI_SRC)
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_CLI_SRC)"
	@$(CC) $(CFLAGS) $(TEST_CLI_SRC) -o $(TEST_CLI_BIN)

$(TEST_PL_BIN): $(TEST_PL_SRC)
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_PL_SRC)"
	@$(CC) $(CFLAGS) $(TEST_PL_SRC) -o $(TEST_PL_BIN)

install: $(TARGET)
	@printf "$(GREEN)  installing vyt -> $(HOME)/.local/bin/$(TARGET)$(RESET)\n"
	@install -Dm755 $(TARGET) $(HOME)/.local/bin/$(TARGET)
	@install -Dm644 man/vyt.1 $(HOME)/.local/share/man/man1/vyt.1

clean:
	@printf "$(YELLOW)  cleaning...$(RESET)\n"
	@rm -f $(TARGET) $(TEST_BINS)
