# PS3 features and the PS4 app

Compared on 2026-11-07: PS4 1.13 (in progress) against JellyFin [PS] for PS3 3.5.1
(`D:\Coding-Projects\PS3\JellyFin-PS3-LosslessAudio`, its `docs/MANUAL.md` and `CHANGELOG.md`).

## Added in 1.13

| PS3 feature | PS4 1.13 | Where |
|---|---|---|
| Music tabs: Albums, Artists, Playlists, Genres, Songs | L2 / R2 in a music library; the same server queries as the PS3 | `app.cpp` `page_path`, `main.cpp` library loop |
| Tracks with no album tag | Listed under Songs | - |
| Shuffle, Up Next, picking from the queue | Triangle shuffles; the next three show; Options opens the queue | `main.cpp` `play_tracks`, `ui_view.cpp` `draw_music_screen` |
| Whole albums in the queue | Up to 1000 tracks (was 60) | `fetch_album_tracks` |
| Live search, type filter, recent searches, details box | PS3 Classic layout: field and keyboard, which folds away for the results; chips with counts; a 450 ms pause before searching, on a worker | `main.cpp` `search_screen`, `ui_view.cpp` `draw_search_page` |
| Audio / subtitle switching in the player | Triangle menu; a switch restarts the stream at the same position | `main.cpp` `play` |
| Downloads: film, episode, whole season; offline playback | Detail page buttons, Downloads on Home, to the console or a USB drive | `downloads.cpp`, `sandbox_ps4.cpp` |
| Media tab: play from USB drives with no server | USB Drive card in My Media, checked every 2 s for drives plugged in or out; folders, videos, a folder's music as a queue | `local_media.cpp`, `main.cpp` `usb_screen` |

**Not yet tested on a PS4:** all of the above was built and previewed on the host
(`tools/test_ui_113.sh`), and the server requests were checked against a Jellyfin 12.2
server. The USB escape (`sandbox_ps4.cpp`) has never been run on a
console: check the log's `USB:` lines, and that sound still plays after
it (MPV opens a sound port for each playback).

## Still missing

- **Subtitles drawn by the console** (PGS and text, 3 fonts, 3 colours). The PS4 still has the server burn them in. MPV has libass, so text subtitles could be drawn here for direct play.
- **Surround** (5.1 / 7.1 LPCM, lossless TrueHD and DTS-HD MA, Dolby Digital to a soundbar, Dialogue Boost). The PS4 plays stereo; its audio path would need multichannel output through SDL / sceAudioOut.
- **24 Hz output.** Depends on the PS4 video-out modes homebrew can set.
- **Quality per title** and a Quality picker on the details page; **player stats overlay**.
- **Favourites row on Home** (the PS4 has Favourites on Square).
- **XMB layout**, menu sounds; **Live TV** Customise tabs, genre tabs from channel names, a guide grid in Classic.
- **Themes** from a user `.ini` (the PS4 reads `/data/JellyFin-PS4/themes/`).
- **Controllers**: Blu-ray remote, USB / Bluetooth keyboards.

## On the PS4 but not the PS3

- XMLTV guide download on the console (Settings › Live TV).
- Screensaver with Home artwork.
- Software Update downloads the package for the Package Installer.
