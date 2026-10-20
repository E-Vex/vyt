#ifndef DOWNLOAD_H
#define DOWNLOAD_H

/*
 * Download YouTube media for `url` via yt-dlp, saving it to the current
 * directory with yt-dlp's default naming.
 *
 * Runs `yt-dlp -- <url>` with stdin/stdout/stderr inherited, so yt-dlp's own
 * progress bar and diagnostics reach the user unchanged.  Returns yt-dlp's
 * exit status (0 on success), the conventional 128+signal when yt-dlp is
 * stopped by SIGINT/SIGTERM, or -1 if vyt itself failed to spawn yt-dlp
 * (a diagnostic is printed by proc.c in that case).
 */
int download_youtube(const char *url);

#endif /* DOWNLOAD_H */
