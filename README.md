# vyt

<p align="center">
  <img src="vyt_logo.svg" alt="vyt logo" width="180"/>
</p>

`vyt` is a small C wrapper around [`yt-dlp`](https://github.com/yt-dlp/yt-dlp)
and [`mpv`](https://mpv.io/) that makes searching, playing and organising
YouTube media fast & simple from the terminal.

`vyt` never shells out through `system()` commands run via `fork` +
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
  vyt playlist <command> ...
  vyt -V
  vyt -h

Options:
  -m URL    Play YouTube URL as music (audio only, via mpv --no-video)
  -v URL    Play YouTube URL as video (via mpv)
  -s QUERY  Search YouTube for QUERY (via yt-dlp, top 10 results)
  -V        Print the version
  -h        Show this message :D

Playlist:
  vyt playlist add <name>                          Create a new playlist
  vyt playlist list                                List all playlists
  vyt playlist remove <name>                       Remove a playlist
  vyt playlist song add <p> <song_name> <url>      Add a named song to a playlist
  vyt playlist song remove <p> <url>               Remove a URL from a playlist
  vyt playlist song list <p>                       List songs in a playlist
  vyt playlist play <p>                            Play a playlist shuffled forever
```

See `man vyt` for the full manual page.

### Examples

```sh
# Play a video
vyt -v "https://youtu.be/pahb4TugTkQ" 

# Play just the audio
vyt -m "https://youtu.be/OgJ5bEg31Aw"

# Search (prints the top 10 results as "title | URL")
vyt -s "Purgatori"
```

## Playlists

`vyt` keeps persistent playlists as plain text files under
`~/.config/vyt/playlists/`. Each playlist is one file, each song is one
line in the form `song_name = url`, and songs' URLs must be `http://`
or `https://`.

If `XDG_CONFIG_HOME` is set and absolute, playlists live under
`$XDG_CONFIG_HOME/vyt/playlists/` instead useful for keeping your
config separate from your home directory or for tests.

### Playlist commands

```sh
# Create a new, empty playlist
vyt playlist add my-playlist

# List every playlist (sorted alphabetically)
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
whitespace; lines that fail this check at playback time are reported
with their line number, so you'll see exactly which entry to fix.

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

## Uninstall

```sh
rm ~/.local/bin/vyt
rm ~/.local/share/man/man1/vyt.1
# Optionally remove your saved playlists:
rm -rf ~/.config/vyt/playlists
```

## LICENSE

[![License: MIT](https://img.shields.io/github/license/gcla/termshark.svg?color=yellow)](LICENSE)
