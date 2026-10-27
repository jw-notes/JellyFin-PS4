# Jellyfin [PS] for PS4 — 1.10

Faster, smoother, and a lot more to do.

## Highlights

- **Starts in about 4 seconds**, and every screen and animation runs smoothly at 60 frames a second (startup used to take around 16 seconds).
- **Direct play**: H264 films and shows in MP4, MKV and MOV, and most music formats, play as they are and start in 1–3 seconds instead of waiting for the server to convert them.
- **Smoother video**, and **1080p playback**, on by default, with Quality at 20 Mbps (both in Settings › Playback).
- **Live TV** channels from an M3U tuner play straight from their source: they start in about 2–4 seconds, in their best quality up to 720p, and fall back to the server when they must.
- The **PS3 buffering screen**, with the ring, the mark and progress.
- **L1 while watching** sets the picture's size, position and Fit / Fill / Stretch, remembered for every video.
- **Music**: a Now Playing screen, albums that play through (L1 / R1 skip tracks) and four visualizers — Bars, Aurora, Halo and Scope (Square switches).
- The clock, the date and your profile picture in the header, and **"Ends at"** times on the details page, the quick peek and the player bar.
- **Languages**: English, Japanese, Portuguese (Brazil), German, French and Spanish.
- **Settings** in four groups — Playback, Display, Account and System — with **Software Update** and a **Debug Logging** switch (off by default).
- Artwork shimmers while it loads, a smoother quick peek, and a new app icon.

## Package

`dist/v1.10/IV0000-JFPS40000_00-JELLYFINPS400000.pkg` — title ID `JFPS40000`, version `01.10`. It installs over 1.09 and keeps your sign-in and settings.

## Known limitations

- Video is decoded in software and audio is stereo. Settings saved by 1.09 keep their quality; 1080p and 20 Mbps apply to new installs, or can be set in Settings › Playback.
- Audio and subtitle tracks are chosen before playback, not during it; choosing a non-default audio track or subtitles has the server convert the file.
- Live TV has no channel up / down inside the player.
- Software Update downloads the new package; it is installed from the Package Installer, not by the app.
