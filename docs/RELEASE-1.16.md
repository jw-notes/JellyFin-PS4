# Jellyfin [PS] for PS4 - 1.16

## New

- **Options while watching.** Options opens a menu over the player, as on the PS4's own player: **Chapters**, **Audio / Subtitles**, **Picture** and **Playback Statistics**, each with what it is set to now. For a show it also has **Next Episode** (with its name), **Previous Episode**, and **Skip Intros** and **Skip Credits** (Off, Button or Auto). Live TV has Picture and Playback Statistics.
- **Chapters.** L2 / R2 jump to the previous or next chapter, as on a Blu-ray player: previous goes back to the start of the chapter you are in, or the one before if you have just started it. Options › Chapters lists them with their names and times and a picture of each, and X plays from there. The chapters are marked on the progress bar, and the chapter's name shows beside the time while you scrub. They come from your server, or from the file itself for USB drives, downloads and films played directly. A film with no chapters keeps L2 / R2 for jumping 5 minutes.
- **Seek previews.** While you scrub, a picture of the scene shows above the bar. It uses the trickplay images your server makes (Jellyfin 10.9 and later); without them, and for USB drives and downloads, the PS4 takes the picture from the file itself.
- **Subtitles in Chinese, Japanese, Korean, Hebrew, Arabic and Persian.** Right-to-left text reads right to left and Arabic letters join. Greek and Russian subtitles now show in Noto Sans and Roboto Condensed too, not only Open Sans.
- **Playback statistics.** Press R3 (the right stick) while anything plays to see the codec, resolution, frame rate, dropped frames, bitrates, audio format, buffer and network speed, and whether the file is played directly or converted by the server, and why. Press it again to hide it; Settings › Playback › Playback Statistics keeps it on.
- **Your sign-in is saved encrypted.** The sign-in token is sealed to your console, so a copy of the file is no use on another console or PC. Sign-ins saved by earlier versions are sealed the first time 1.16 opens; you do not need to sign in again. Your password is never saved.
- **The JELLYFIN lockup as on the PS3.** The header shows the mark and the JELLYFIN wordmark in your theme's colours, and the start-up animation docks into it.
- **Button hints light the right directions.** The d-pad in the hints lights left and right, or up and down, where only those do something: seek and volume in music, the audio and subtitle menu, settings, the A-Z bar and others.

## Fixed

- Settings › About › Updates listed the 1.13 changes under 1.14. Both are listed correctly now.
- Crash logs and the player's requests carried old version numbers; they now carry the real one.

## Known limits

- Chinese and Japanese share one font, so a few characters are drawn in the Simplified Chinese style in Japanese or Traditional Chinese text.
- Without trickplay images on the server, a seek preview can take a moment to appear, longer for large 4K files.

## Package

`IV0000-JFPS40000_00-JELLYFINPS400000.pkg` - title ID `JFPS40000`, version `01.16`. Installs over 1.15 and keeps your sign-in and settings.
