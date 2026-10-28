# Jellyfin [PS] for PS4 — 1.12

## New

- **On Now on Home**, as in Jellyfin's web app: what's on Live TV now, with the show's picture, its episode, its times and how far in it is. X plays the channel. Uses the server's guide, or the one downloaded to the PS4; turn it off in Settings › Live TV.

## Fixed

- **Software Update downloads again.** GitHub serves release files from a server the console's own web connection could not reach; the app now falls back to its built-in HTTPS client. The download is still checked against GitHub's published size and SHA-256.
- **Live TV no longer crashes after repeated reconnects.** Reconnecting to a channel uses far less memory, and happens at most twice a minute.
- The Live TV guide downloads through the same fallback when the console's connection can't reach its address.

1.11 can't download this update by itself (its downloader is the one being fixed): install this package once from the Package Installer, and later updates download in the app.

## Package

`IV0000-JFPS40000_00-JELLYFINPS400000.pkg` — title ID `JFPS40000`, version `01.12`. Installs over 1.11 and keeps your sign-in and settings.
