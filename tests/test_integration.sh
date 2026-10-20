#!/usr/bin/env bash
#
# Integration tests for vyt's terminal output.
#
# These exercise the real binary end to end with deterministic fixtures:
# stub yt-dlp/mpv executables on PATH stand in for the real tools, and
# XDG_CONFIG_HOME points storage at a scratch directory.  No network
# access is required or performed.
#
# Every assertion runs vyt with stdout/stderr redirected, so color is
# verified in both directions: absent by default, present under
# CLICOLOR_FORCE (unless NO_COLOR overrides it).

set -u

cd "$(dirname "$0")/.." || exit 1
BIN=./vyt

if [ ! -x "$BIN" ]; then
    echo "integration: $BIN is not built (run make first)" >&2
    exit 1
fi

checks=0
failures=0

# Every vyt invocation runs under a timeout so a wedged child fails one
# check instead of stalling the whole suite (also guards CI runners).
VYT="timeout -k 1 10"

ok() {
    checks=$((checks + 1))
}

bad() {
    checks=$((checks + 1))
    failures=$((failures + 1))
    echo "FAIL: $*" >&2
}

# expect_status <want> <description> <cmd...>
expect_status() {
    local want="$1" desc="$2"
    shift 2
    $VYT "$@" >/dev/null 2>&1
    local got=$?
    if [ "$got" = "$want" ]; then ok; else bad "$desc: exit $got, want $want"; fi
}

# expect_esc_err: CLICOLOR_FORCE must produce escapes on stderr.
expect_esc_err() {
    local desc="$1"
    shift
    if $VYT "$@" 2>&1 >/dev/null | LC_ALL=C grep -q "$(printf '\033')"; then
        ok
    else
        bad "$desc: stderr has no ANSI escapes despite CLICOLOR_FORCE"
    fi
}

# expect_out <pattern> <description> <cmd...>
expect_out() {
    local pattern="$1" desc="$2"
    shift 2
    if $VYT "$@" 2>/dev/null | grep -q -- "$pattern"; then ok; else bad "$desc: stdout missing '$pattern'"; fi
}

# expect_err <pattern> <description> <cmd...>
expect_err() {
    local pattern="$1" desc="$2"
    shift 2
    if $VYT "$@" 2>&1 >/dev/null | grep -q -- "$pattern"; then ok; else bad "$desc: stderr missing '$pattern'"; fi
}

# expect_no_esc_out / expect_no_esc_err: ANSI escapes must never appear.
expect_no_esc_out() {
    local desc="$1"
    shift
    if $VYT "$@" 2>/dev/null | LC_ALL=C grep -q "$(printf '\033')"; then
        bad "$desc: stdout contains ANSI escapes"
    else
        ok
    fi
}

expect_no_esc_err() {
    local desc="$1"
    shift
    if $VYT "$@" 2>&1 >/dev/null | LC_ALL=C grep -q "$(printf '\033')"; then
        bad "$desc: stderr contains ANSI escapes"
    else
        ok
    fi
}

# expect_esc_out: CLICOLOR_FORCE must produce escapes on stdout.
expect_esc_out() {
    local desc="$1"
    shift
    if $VYT "$@" 2>/dev/null | LC_ALL=C grep -q "$(printf '\033')"; then
        ok
    else
        bad "$desc: stdout has no ANSI escapes despite CLICOLOR_FORCE"
    fi
}

# assert_file <file> <pattern> <description>
assert_file() {
    local file="$1" pattern="$2" desc="$3"
    if grep -q -- "$pattern" "$file" 2>/dev/null; then ok; else bad "$desc: file missing '$pattern'"; fi
}

assert_file_no_esc() {
    local file="$1" desc="$2"
    if LC_ALL=C grep -q "$(printf '\033')" "$file" 2>/dev/null; then
        bad "$desc: file contains ANSI escapes"
    else
        ok
    fi
}

# ---------------------------------------------------------------------------

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

export XDG_CONFIG_HOME="$TMP/config"
export LC_ALL=C
unset NO_COLOR CLICOLOR CLICOLOR_FORCE TERM

FAKE="$TMP/bin"
mkdir -p "$FAKE"

# Stub mpv: plays briefly, succeeds.  Keeps playlist play slow enough to
# observe and fast enough to finish.
cat > "$FAKE/mpv" <<'EOF'
#!/bin/sh
sleep 0.1
exit 0
EOF
chmod +x "$FAKE/mpv"

# Stub yt-dlp used for the "success" search fixture: three result lines in
# vyt's internal field format (unit separator \037).
cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
printf 'Kavinsky - Nightcall\037256\037Kavinsky\037https://www.youtube.com/watch?v=S0\n'
printf 'Second Result\037NA\037NA\037https://youtu.be/second\n'
printf 'Third\03761\037Someone\037https://youtu.be/third\n'
exit 0
EOF
chmod +x "$FAKE/yt-dlp"

# ---------------------------------------------------------------------------
# CLI surface
# ---------------------------------------------------------------------------

expect_status 1 "no arguments exits 1" "$BIN"
expect_err "Usage:" "no arguments prints usage" "$BIN"
expect_status 0 "-h exits 0" "$BIN" -h
expect_out "Options:" "-h shows options" "$BIN" -h
expect_status 0 "-V exits 0" "$BIN" -V
expect_out "vyt 1.3.0" "-V prints version" "$BIN" -V
expect_status 1 "unknown option exits 1" "$BIN" -x
expect_err "unknown option" "unknown option explains itself" "$BIN" -x
expect_status 1 "empty -m argument exits 1" "$BIN" -m ""
expect_err "non-empty" "empty -m explains itself" "$BIN" -m ""
expect_status 1 "empty -s argument exits 1" "$BIN" -s ""
expect_err "non-empty" "empty -s explains itself" "$BIN" -s ""
expect_no_esc_out "-h piped has no escapes" "$BIN" -h
expect_no_esc_err "error piped has no escapes" "$BIN" -x

# ---------------------------------------------------------------------------
# playlist lifecycle, with exit codes
# ---------------------------------------------------------------------------

expect_status 0 "playlist add succeeds" "$BIN" playlist add chill
expect_status 0 "second playlist add succeeds" "$BIN" playlist add second
expect_out "created playlist 'third'" "playlist add confirms" "$BIN" playlist add third
timeout -k 1 10 "$BIN" playlist remove third >/dev/null 2>&1
timeout -k 1 10 "$BIN" playlist remove second >/dev/null 2>&1

expect_status 1 "duplicate playlist add fails" "$BIN" playlist add chill
expect_err "already exists" "duplicate add explains itself" "$BIN" playlist add chill
expect_err "vyt: hint:" "duplicate add offers a hint" "$BIN" playlist add chill

expect_status 1 "invalid playlist name fails" "$BIN" playlist add "a/b"
expect_err "invalid playlist name" "invalid name explains itself" "$BIN" playlist add "a/b"

expect_status 1 "missing playlist fails" "$BIN" playlist song list nope
expect_err "no playlist named 'nope'" "missing playlist explains itself" "$BIN" playlist song list nope

expect_status 0 "song add succeeds" "$BIN" playlist song add chill "Nightcall" "https://youtu.be/AAAA"
expect_status 0 "second song add succeeds" "$BIN" playlist song add chill "Teardrop" "https://youtu.be/BBBB"
expect_status 0 "third song add succeeds" "$BIN" playlist song add chill "Third" "https://youtu.be/CCCC"
expect_out "added 'Fourth' to 'chill'" "song add confirms song and playlist" "$BIN" playlist song add chill "Fourth" "https://youtu.be/DDDD"
timeout -k 1 10 "$BIN" playlist song remove chill "https://youtu.be/DDDD" >/dev/null 2>&1

expect_status 1 "duplicate song add fails" "$BIN" playlist song add chill "Again" "https://youtu.be/AAAA"
expect_err "already in the playlist" "duplicate song explains itself" "$BIN" playlist song add chill "Again" "https://youtu.be/AAAA"
expect_err "song remove chill https://youtu.be/AAAA" "duplicate song hint is copy-pasteable" "$BIN" playlist song add chill "Again" "https://youtu.be/AAAA"

expect_status 1 "bad URL rejected" "$BIN" playlist song add chill "X" "notaurl"
expect_err "invalid URL" "bad URL explains itself" "$BIN" playlist song add chill "X" "notaurl"

expect_status 1 "song remove of missing URL fails" "$BIN" playlist song remove chill "https://youtu.be/ZZZ"
expect_err "is not in the playlist" "missing song explains itself" "$BIN" playlist song remove chill "https://youtu.be/ZZZ"

expect_status 0 "song remove succeeds" "$BIN" playlist song remove chill "https://youtu.be/CCCC"
expect_out "removed https://youtu.be/BBBB from 'chill'" "song remove confirms" "$BIN" playlist song remove chill "https://youtu.be/BBBB"

expect_status 1 "unknown playlist command fails" "$BIN" playlist frobnicate
expect_err "unknown playlist command" "unknown command explains itself" "$BIN" playlist frobnicate
expect_err "vyt playlist help" "unknown command points to help" "$BIN" playlist frobnicate

expect_status 0 "playlist help exits 0" "$BIN" playlist help
expect_out "Create a new playlist" "playlist help describes commands" "$BIN" playlist help

expect_status 1 "wrong arg count fails" "$BIN" playlist song add chill
expect_err "usage:" "wrong arg count shows usage" "$BIN" playlist song add chill

# ---------------------------------------------------------------------------
# scriptable (piped) listing formats
# ---------------------------------------------------------------------------

timeout -k 1 10 "$BIN" playlist add solo >/dev/null 2>&1
timeout -k 1 10 "$BIN" playlist song add solo "Only" "https://youtu.be/ONLY" >/dev/null 2>&1

expect_out "^chill$" "playlist list piped prints bare names" "$BIN" playlist list
if [ "$(timeout -k 1 10 "$BIN" playlist list 2>/dev/null)" = "chill
solo" ]; then ok; else bad "playlist list piped: exact output expected"; fi

expect_out "^Nightcall = https://youtu.be/AAAA$" "song list piped keeps storage format" "$BIN" playlist song list chill
if [ "$(timeout -k 1 10 "$BIN" playlist song list chill 2>/dev/null)" = "Nightcall = https://youtu.be/AAAA" ]; then
    ok
else
    bad "song list piped: exact output expected"
fi

expect_no_esc_out "song list piped has no escapes" "$BIN" playlist song list chill

# Empty playlist: exit 0, note on stderr.
timeout -k 1 10 "$BIN" playlist add hollow >/dev/null 2>&1
expect_status 0 "empty playlist song list exits 0" "$BIN" playlist song list hollow
expect_err "has no songs" "empty song list explains itself" "$BIN" playlist song list hollow
expect_err "vyt: hint:" "empty song list offers next step" "$BIN" playlist song list hollow
expect_status 0 "empty playlists dir note exits 0" env XDG_CONFIG_HOME="$TMP/unused" "$BIN" playlist list
expect_err "no playlists yet" "empty collection explains itself" env XDG_CONFIG_HOME="$TMP/unused" "$BIN" playlist list

# ---------------------------------------------------------------------------
# search via stub yt-dlp
# ---------------------------------------------------------------------------

export PATH="$FAKE:$PATH"

expect_status 0 "search succeeds" "$BIN" -s "purgatori"
expect_out "^Kavinsky - Nightcall | https://www.youtube.com/watch?v=S0$" \
    "search piped keeps legacy format" "$BIN" -s "purgatori"

lines="$(timeout -k 1 10 "$BIN" -s q 2>/dev/null | wc -l)"
if [ "$lines" = "3" ]; then ok; else bad "search piped: expected 3 lines, got $lines"; fi
expect_no_esc_out "search piped has no escapes" "$BIN" -s q
if [ "$(timeout -k 1 10 "$BIN" -s q 2>/dev/null | grep -c "results for")" = "0" ]; then
    ok
else
    bad "search piped: no header expected"
fi

# Failure: stub exits 3 after printing a diagnostic.
cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
echo "yt-dlp simulated failure" >&2
exit 3
EOF
chmod +x "$FAKE/yt-dlp"
expect_status 3 "search failure passes status through" "$BIN" -s q
expect_err "yt-dlp exited with status 3" "search failure explains itself" "$BIN" -s q
expect_err "update yt-dlp" "search failure hints at yt-dlp updates" "$BIN" -s q

# Empty results: stub exits 0 with no output.
cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$FAKE/yt-dlp"
expect_status 0 "empty search exits 0" "$BIN" -s "nothing matches"
expect_err "no results for" "empty search explains itself" "$BIN" -s "nothing matches"
expect_err "vyt: hint:" "empty search offers a next step" "$BIN" -s "nothing matches"

# Missing yt-dlp: conventional 127 with the not-found diagnostic.
expect_status 127 "missing yt-dlp exits 127" env PATH="$TMP/emptybin" "$BIN" -s q
expect_err "not found" "missing yt-dlp explains itself" env PATH="$TMP/emptybin" "$BIN" -s q

# Malformed lines are skipped and reported, good lines still shown.
cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
printf 'garbage line without separators\n'
printf 'Good\03710\037C\037https://youtu.be/good\n'
exit 0
EOF
chmod +x "$FAKE/yt-dlp"
expect_status 0 "search with malformed lines succeeds" "$BIN" -s q
expect_err "skipped 1 malformed" "malformed lines are reported" "$BIN" -s q
expect_out "Good | https://youtu.be/good" "good lines survive malformed ones" "$BIN" -s q

# ---------------------------------------------------------------------------
# download via stub yt-dlp
# ---------------------------------------------------------------------------

# Stub yt-dlp for download: record the invocation, create an output file,
# and exit 0.  Invoked as: yt-dlp -- <URL>
cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" > yt-dlp-download-args
touch downloaded-by-vyt
exit 0
EOF
chmod +x "$FAKE/yt-dlp"

expect_status 0 "download succeeds" "$BIN" -d "https://youtu.be/abc"
if [ -f downloaded-by-vyt ]; then ok; else bad "download: yt-dlp did not create output file"; fi
if grep -q "https://youtu.be/abc" yt-dlp-download-args; then ok; else bad "download: URL passed to yt-dlp"; fi
if grep -q -- "--" yt-dlp-download-args; then ok; else bad "download: -- separator passed to yt-dlp"; fi
rm -f downloaded-by-vyt yt-dlp-download-args

# Download failure: stub exits non-zero after printing a diagnostic.
cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
echo "yt-dlp simulated download failure" >&2
exit 5
EOF
chmod +x "$FAKE/yt-dlp"
expect_status 5 "download failure passes status through" "$BIN" -d "https://youtu.be/abc"
expect_err "yt-dlp simulated download failure" "download failure surfaces yt-dlp's diagnostic" "$BIN" -d "https://youtu.be/abc"

# Missing yt-dlp: conventional 127 with the not-found diagnostic.
expect_status 127 "missing yt-dlp download exits 127" env PATH="$TMP/emptybin" "$BIN" -d q
expect_err "not found" "missing yt-dlp download explains itself" env PATH="$TMP/emptybin" "$BIN" -d q

# ---------------------------------------------------------------------------
# color behavior
# ---------------------------------------------------------------------------

expect_esc_out "CLICOLOR_FORCE colorizes -h" env CLICOLOR_FORCE=1 "$BIN" -h
expect_no_esc_out "NO_COLOR beats CLICOLOR_FORCE" env CLICOLOR_FORCE=1 NO_COLOR=1 "$BIN" -h
expect_no_esc_out "plain pipe has no escapes" "$BIN" -h
expect_esc_err "CLICOLOR_FORCE colorizes errors" env CLICOLOR_FORCE=1 "$BIN" playlist song list nope
expect_no_esc_err "NO_COLOR silences errors" env CLICOLOR_FORCE=1 NO_COLOR=1 "$BIN" playlist song list nope

# ---------------------------------------------------------------------------
# playback via stub mpv (session-level smoke test)
# ---------------------------------------------------------------------------

# One-song playlist: stub mpv exits 0 quickly, so vyt loops until the
# stop signal.  timeout sends TERM; vyt's handler stops between songs and
# prints the stopped line.
timeout_out="$TMP/timeout.out"
timeout -k 2 3 "$BIN" playlist play solo >"$timeout_out" 2>/dev/null
assert_file "$timeout_out" "shuffling 'solo' (1 song)" "play announces the session"
assert_file "$timeout_out" "now playing \\[1/1\\]" "play announces the current song"
assert_file "$timeout_out" "stopped" "play stops cleanly on signal"

# Playback failure warning: mpv exits non-zero but vyt continues.
cat > "$FAKE/mpv" <<'EOF'
#!/bin/sh
exit 7
EOF
chmod +x "$FAKE/mpv"
warn_out="$TMP/warn.err"
timeout -k 2 3 "$BIN" playlist play solo >/dev/null 2>"$warn_out"
assert_file "$warn_out" "mpv exited with status 7" "playback failure is reported"
assert_file "$warn_out" "vyt: warning:" "playback failure is a warning, not an error"
assert_file "$warn_out" "skipping to the next song" "playback failure explains recovery"

# ---------------------------------------------------------------------------
# tty-shaped output (opt-in: VYT_TTY_TESTS=1)
#
# These checks drive vyt through a pseudo-terminal with script(1) and
# verify the interactive formats.  They are off by default because a pty
# nested inside some CI/harness monitors can deadlock; the same formats
# are covered by the unit suites through explicit pretty= arguments.
# ---------------------------------------------------------------------------

if [ "${VYT_TTY_TESTS:-0}" = "1" ] && command -v script >/dev/null 2>&1; then
    cat > "$FAKE/yt-dlp" <<'EOF'
#!/bin/sh
printf 'Only Result\03761\037Chan\037https://youtu.be/only\n'
exit 0
EOF
    chmod +x "$FAKE/yt-dlp"
    tty_out="$TMP/tty.out"
    if timeout -k 1 15 script -qec "$BIN -s q" "$tty_out" >/dev/null 2>&1; then
        assert_file "$tty_out" "1 result for 'q':" "tty search prints a header"
        assert_file "$tty_out" "Only Result" "tty search shows the title"
        assert_file "$tty_out" "1:01 Â· Chan" "tty search shows duration and channel"
        assert_file "$tty_out" "play one with: vyt -m <URL>" "tty search has a footer hint"
    else
        echo "note: script(1) could not allocate a pty; tty checks skipped" >&2
    fi

    tty_list="$TMP/ttylist.out"
    if timeout -k 1 15 script -qec "$BIN playlist list" "$tty_list" >/dev/null 2>&1; then
        assert_file "$tty_list" "1 song" "tty playlist list shows song counts"
        assert_file "$tty_list" "empty" "tty playlist list marks empty playlists"
    else
        echo "note: script(1) could not allocate a pty; tty checks skipped" >&2
    fi
else
    echo "note: tty checks skipped (set VYT_TTY_TESTS=1 and have script(1) available)" >&2
fi

# ---------------------------------------------------------------------------

echo "integration: $checks checks, $failures failures"
if [ "$failures" -ne 0 ]; then
    exit 1
fi
exit 0
