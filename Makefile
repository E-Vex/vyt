CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -Wpedantic -std=c17 -O2
SRC      := $(wildcard src/*.c)
HDRS     := $(wildcard src/*.h)
TARGET   := vyt

# Each test binary links only the source files it actually exercises.
# This keeps compile times low and surfaces missing dependencies early.
TEST_CLI_SRC    := tests/test_cli.c src/cli.c src/output.c
TEST_CLI_BIN    := tests/test_cli
TEST_PL_SRC     := tests/test_playlist.c src/playlist.c src/output.c
TEST_PL_BIN     := tests/test_playlist
TEST_OUT_SRC    := tests/test_output.c src/output.c
TEST_OUT_BIN    := tests/test_output
TEST_PROC_SRC   := tests/test_proc.c src/proc.c src/output.c
TEST_PROC_BIN   := tests/test_proc
TEST_SEARCH_SRC := tests/test_search.c src/search.c src/proc.c src/output.c
TEST_SEARCH_BIN := tests/test_search
TEST_BINS       := $(TEST_CLI_BIN) $(TEST_PL_BIN) $(TEST_OUT_BIN) $(TEST_PROC_BIN) $(TEST_SEARCH_BIN)

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

# Headers are explicit dependencies: editing a .h re-rebuilds everything
# that could include it, instead of leaving a stale binary behind.
$(TARGET): $(SRC) $(HDRS)
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(SRC)"
	@$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

test: banner $(TARGET) $(TEST_BINS)
	@printf "$(YELLOW)  running tests...$(RESET)\n"
	@for bin in $(TEST_BINS); do \
	        printf "$(CYAN)  RUN$(RESET) %s\n" "$$bin"; \
	        ./$$bin || exit 1; \
	done
	@printf "$(CYAN)  RUN$(RESET) tests/test_integration.sh\n"
	@bash tests/test_integration.sh

$(TEST_CLI_BIN): $(TEST_CLI_SRC) src/cli.h src/output.h src/version.h
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_CLI_SRC)"
	@$(CC) $(CFLAGS) $(TEST_CLI_SRC) -o $(TEST_CLI_BIN)

$(TEST_PL_BIN): $(TEST_PL_SRC) src/playlist.h src/output.h
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_PL_SRC)"
	@$(CC) $(CFLAGS) $(TEST_PL_SRC) -o $(TEST_PL_BIN)

$(TEST_OUT_BIN): $(TEST_OUT_SRC) src/output.h
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_OUT_SRC)"
	@$(CC) $(CFLAGS) $(TEST_OUT_SRC) -o $(TEST_OUT_BIN)

$(TEST_PROC_BIN): $(TEST_PROC_SRC) src/proc.h src/output.h
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_PROC_SRC)"
	@$(CC) $(CFLAGS) $(TEST_PROC_SRC) -o $(TEST_PROC_BIN)

$(TEST_SEARCH_BIN): $(TEST_SEARCH_SRC) src/search.h src/proc.h src/output.h
	@printf "$(CYAN)  CC$(RESET)  %s\n" "$(TEST_SEARCH_SRC)"
	@$(CC) $(CFLAGS) $(TEST_SEARCH_SRC) -o $(TEST_SEARCH_BIN)

install: $(TARGET)
	@printf "$(GREEN)  installing vyt -> $(HOME)/.local/bin/$(TARGET)$(RESET)\n"
	@install -Dm755 $(TARGET) $(HOME)/.local/bin/$(TARGET)
	@install -Dm644 man/vyt.1 $(HOME)/.local/share/man/man1/vyt.1

clean:
	@printf "$(YELLOW)  cleaning...$(RESET)\n"
	@rm -f $(TARGET) $(TEST_BINS)
