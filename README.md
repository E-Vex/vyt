# vyt

<p align="center">
  <img src="vyt_logo.svg" alt="vyt logo" width="180"/>
</p>

`vyt` is a small C wrapper around [`yt-dlp`](https://github.com/yt-dlp/yt-dlp)
and [`mpv`](https://mpv.io/) that makes searching, playing and organising
YouTube media fast & simple from the terminal.

`vyt` never shells out through `system()`; commands run via `fork` +
`execvp`, so URLs and search queries are passed as arguments, not
interpolated into a shell string.

**Platform:** Linux only

## Build & install

```sh
git clone https://github.com/E-Vex/vyt.git
cd vyt
make            # builds ./vyt
make install    # installs to ~/.local/bin/vyt 
                # and the man page to ~/.local/share/man/man1/vyt.1
```

Make sure `~/.local/bin` is on your `PATH`.

## Usage

```
Usage:
  vyt -m URL
  vyt -v URL
  vyt -s QUERY
  vyt -d URL
  vyt playlist <command> ...
  vyt -V
  vyt -h

Options:
  -m, --music URL     Play YouTube URL as music (audio only, via mpv --no-video)
  -v, --video URL     Play YouTube URL as video (via mpv)
  -s, --search QUERY  Search YouTube for QUERY (via yt-dlp, top 10 results)
  -d, --download URL  Download YouTube media via yt-dlp to the current directory
  -V, --version       Print the version
  -h, --help          Show this message :D

Playlist:
  vyt playlist add <name>                          Create a new playlist
  vyt playlist list                                List all playlists
  vyt playlist remove <name>                       Remove a playlist
  vyt playlist play <name>                         Play a playlist shuffled forever
  vyt playlist song add <p> <song_name> <url>      Add a named song to a playlist
  vyt playlist song remove <p> <url>               Remove a URL from a playlist
  vyt playlist song list <p>                       List songs in a playlist
```

Every option has a long spelling (`--music`, `--video`, `--search`,
`--download`, `--help`, `--version`); the short ones stay the documented
way to use `vyt`.

See `man vyt` for the full manual page.

### Examples

```sh
# Play a video
vyt -v "https://youtu.be/pahb4TugTkQ" 

# Play just the audio
vyt -m "https://youtu.be/OgJ5bEg31Aw"

# Search (prints the top 10 results as a numbered list)
vyt -s "Purgatori"

# Download media to the current directory
vyt -d "https://youtu.be/pahb4TugTkQ"
```

### Media references

`vyt` accepts a full link, a YouTube link without its scheme, or a bare
video ID — all three of these play the same video:

```sh
vyt -m "https://youtu.be/dQw4w9WgXcQ"
vyt -m "youtu.be/dQw4w9WgXcQ"
vyt -m "dQw4w9WgXcQ"
```

Anything else is refused with a clear error instead of being handed to
`mpv` to fail obscurely:

```
$ vyt -m "not a url"
vyt: error: 'not a url' is not a YouTube URL or video ID
vyt: hint: expected a link such as https://youtu.be/dQw4w9WgXcQ, or a video ID
```

## Terminal output

Every message from `vyt` follows the same shape, so you can tell at a
glance what happened:

```
vyt: ✓ created playlist 'chill'        success (green check mark)
vyt: no playlists yet                  neutral status
vyt: error: no playlist named 'nope'   failure (red label)
vyt: warning: skipping entry '...'     recoverable problem (yellow label)
vyt: hint: run 'vyt playlist list'     suggested next step (cyan label)
```

Errors say what failed and, when the cause is known, why — and most of
them end with a `vyt: hint:` line containing a command you can copy and
paste to move forward:

```
$ vyt playlist song add chill "Nightcall" https://youtu.be/AAAA
vyt: error: cannot add to 'chill': https://youtu.be/AAAA is already in the playlist
vyt: hint: remove it first with 'vyt playlist song remove chill https://youtu.be/AAAA'
```

### Colors

Colors are enabled only when stdout/stderr is an interactive terminal,
and can be controlled with the usual environment conventions:

- `NO_COLOR` (non-empty): disables color entirely — wins over everything
- `CLICOLOR_FORCE` (non-empty, not `0`): forces color even when piped
- `CLICOLOR=0`: disables color
- `TERM=dumb`: disables color

With color off, output is plain text with the same wording — nothing is
lost except the escapes.

### Scriptable output

When stdout is **not** a terminal (pipes, redirection, scripts), the
listing commands switch to stable, easy-to-parse formats so existing
shell scripts keep working unchanged:

- `vyt -s QUERY` prints one `title | URL` line per result — exactly what
  older versions printed — with no header or footer
- `vyt playlist list` prints one playlist name per line, nothing else
- `vyt playlist song list <p>` prints the storage format, one
  `song_name = URL` line per song

Messages, warnings, errors and hints always go to stderr (or, for
success confirmations and playback status, to stdout), and exit codes
are unchanged.

## Playlists

`vyt` keeps persistent playlists as plain text files under
`~/.config/vyt/playlists/`. Each playlist is one file, each song is one
line in the form `song_name = url`, and songs' URLs must be `http://`
or `https://`.

If `XDG_CONFIG_HOME` is set and absolute, playlists live under
`$XDG_CONFIG_HOME/vyt/playlists/` instead, useful for keeping your
config separate from your home directory or for tests.

### Playlist commands

```sh
# Create a new, empty playlist
vyt playlist add my-playlist

# List every playlist (sorted alphabetically, with song counts)
vyt playlist list

# Delete a playlist (the file is removed; songs are not recoverable)
vyt playlist remove my-playlist

# Add a URL to a playlist (duplicates are rejected)
vyt playlist song add my-playlist "Nightcall" https://youtu.be/abc

# Remove a URL from a playlist
vyt playlist song remove my-playlist https://youtu.be/abc

# Show every song in a playlist
vyt playlist song list my-playlist
```

### Listing output

On a terminal, `vyt playlist list` aligns names in a column with song
counts, and `vyt playlist song list` numbers each song with the URL on
its own dimmed line, so long names and long URLs never fight for space:

```
$ vyt playlist list
chill      3 songs
focus      empty
workout    12 songs

$ vyt playlist song list chill
2 songs in 'chill':
1. Nightcall
   https://youtu.be/AAAA
2. Teardrop
   https://youtu.be/BBBB
```

### Playing a playlist

```sh
vyt playlist play my-playlist
```

`play` reads the playlist, picks songs in **random order**, and hands
each one to `mpv --no-video` (the same path as `vyt -m`). When a song
finishes, the next random song starts automatically. Playback continues
forever until you press **Ctrl+C**.

When more than one song is in the playlist, `vyt` will never pick the
same song twice in a row. Randomness comes from the kernel CRNG via
`getrandom(2)`, so two consecutive `vyt playlist play` runs will not
produce the same order.

If `mpv` exits with a non-zero status for one song (network blip,
age-restricted video, dead URL, …), `vyt` prints a warning and moves on
to the next random song rather than killing the whole session.

### Hand-editing playlist files

Playlists are plain text, so you can edit them directly:

```sh
$EDITOR ~/.config/vyt/playlists/chill
```

Each line is one song, in the form `song_name = url`. Song names may
contain spaces and even `=` characters; the parser splits on the *last*
` = ` in the line, so a name like `Song = Cool` round-trips correctly.
Lines that don't contain ` = ` are treated as bare URLs (legacy entries
written by older versions of `vyt`); they are loaded with an empty name
and displayed as just the URL.

Blank lines and leading/trailing whitespace are ignored on load. The
URL portion must start with `http://` or `https://` and contain no
whitespace; lines that fail this check are reported with their line
number and skipped on load, so you'll see exactly which entry to fix.

### Storage layout

```
~/.config/vyt/playlists/
├── chill
├── workout
└── focus
```

Each file looks like:

```
Nightcall = https://youtu.be/AAAA
Teardrop = https://youtu.be/BBBB
Focus = https://youtu.be/CCCC
```

`vyt` creates the playlists directory on first use with `mkdir -p`
semantics, so `vyt playlist add` works on a fresh install.

### Atomic writes

Adding or removing a song writes the new playlist to a temporary file
in the same directory, calls `fsync(2)` on it, and then `rename(2)`s it
over the original. This means an interrupted `vyt` (power cut, `kill -9`,
full disk) can never leave a half-written playlist file, you either
have the old version or the new version, never a mix.

## Dependencies

- [`yt-dlp`](https://github.com/yt-dlp/yt-dlp) and [`mpv`](https://mpv.io/),
  both available on your `PATH`
- A C17 compiler (`gcc` or `clang`)
- `make`
- Linux (not currently tested/supported on macOS or Windows)

## Tests

```sh
make test
```

This builds and runs the unit suites (CLI parsing, playlist storage,
terminal-output formatting, subprocess handling, search formatting, URL
normalization) and
an integration suite that exercises the real binary end to end against
stub `yt-dlp`/`mpv` executables — no network access is needed. The
interactive (terminal-width) listing formats can additionally be
checked through a pseudo-terminal with `VYT_TTY_TESTS=1 bash
tests/test_integration.sh` where `script(1)` is available.

## Uninstall

```sh
rm ~/.local/bin/vyt
rm ~/.local/share/man/man1/vyt.1
# Optionally remove your saved playlists:
rm -rf ~/.config/vyt/playlists
```

## LICENSE

[![License: MIT](https://img.shields.io/github/license/E-Vex/vyt.svg?color=yellow)](LICENSE)
