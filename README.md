# vyt

vyt is a small C wrapper around [`yt-dlp`](https://github.com/yt-dlp/yt-dlp) and [`mpv`](https://mpv.io/) that makes searching and playing YouTube media fast & simple from the terminal with just 3 options:

-s search
-m play music
-v play video

vyt never shells out through `system()` — commands run via `fork` + `execvp`,
so URLs and search queries are passed as arguments, not interpolated into a
shell string.

## Build & install

```sh
git clone https://github.com/E-Vex/vyt.git
cd vyt
make            # builds ./vyt
make install    # installs to ~/.local/bin/vyt
```

## Usage

```
Usage:
  vyt -m URL
  vyt -v URL
  vyt -s QUERY
  vyt -h

Options:
  -m URL    Play YouTube URL as music (audio only, via mpv --no-video)
  -v URL    Play YouTube URL as video (via mpv)
  -s QUERY  Search YouTube for QUERY (via yt-dlp, top 10 results)
  -V        Print the version
  -h        Show this message :D
```

## Dependencies

- [`yt-dlp`](https://github.com/yt-dlp/yt-dlp) and [`mpv`](https://mpv.io/),
  both available on your `PATH`
- A C17 compiler (`gcc` or `clang`)
- `make`

Make sure `~/.local/bin` is on your `PATH`.

## Tests

```sh
make test
```



## License

MIT — see [LICENSE](LICENSE).
