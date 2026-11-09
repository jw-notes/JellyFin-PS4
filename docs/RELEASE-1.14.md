# Jellyfin [PS] for PS4 - 1.14

## New, from the PS3 version

- **Subtitles drawn by the console.** SubRip, WebVTT, ASS and PGS subtitles are drawn over the picture by the PS4 itself, whether they are inside the file or a separate file on your server. A film with subtitles now plays straight from the server instead of being converted, so it starts faster and looks as it should. The server only burns subtitles into the picture when it has to convert the video anyway and they are pictures (PGS, DVD).
- **Settings › Subtitles.** **Subtitle Font**: Open Sans (the default), Noto Sans or Roboto Condensed, all bold. **Subtitle Colour**: White, Soft Yellow or Soft Grey, each with its own outline. **Draw Subtitles**: Console, or Server to have them burned into the picture as before. ASS subtitles keep their own styling.

## New

- **Subtitle files beside a video on a USB drive.** `Film.en.srt`, `Film.fr.srt` and the like are listed by language in the Triangle menu, next to the file's own tracks. Pick any of them while the film plays and it switches straight away; Off turns them off again.
- **Audio and subtitle tracks on USB files and downloads** switch in place from the Triangle menu, without starting the file again.
- **Downloads to a USB drive work on more consoles.** When GoldHEN cannot lift the app's sandbox, downloads are written to `JellyFin/` on the drive through GoldHEN's FTP server, and resume from where they stopped. FTP must be on in GoldHEN's settings.
- **USB drives show up on more consoles**, read through GoldHEN's FTP server when the sandbox stays on. FTP must be on in GoldHEN's settings.
- **Button prompts from the PlayStation controller sheet.** Options and the touchpad are shown as the real buttons, and the d-pad shows which directions a screen uses, lit in your theme's colour.

## Audio

- **Dolby Digital (AC-3) and Dolby Digital Plus (E-AC-3) are as loud as AAC.** 5.1 Dolby tracks were mixed down to stereo about 7 dB quieter than the same mix in AAC. All 5.1 sound is now mixed the same way, with a limiter that holds loud passages under full scale.
- **The audio track the details page shows is the one that plays**, and a chosen audio track plays straight from the server when the PS4 can decode it, instead of always being converted.
- **A file whose sound cannot be decoded** goes back to the server to have its sound converted, from the same point. On a USB drive, where there is no server, it says so and plays the picture.

## Fixed

- **The scrub bar on USB files.** The bar stayed empty and a seek could land past the end of the file, which stopped it. The bar now fills and seeks stop short of the end.
- **QuickTime (.mov) and MP4 files on a USB drive** that keep their index at the end of the file loaded and then played nothing. They now play and seek.
- **Crashes when playing a .mov or seeking a download on a USB drive** should no longer happen.
- **Subtitles burned in by the server.** A converted film with PGS subtitles played without them, and asking for its second subtitle track could burn in the first. Both should no longer happen.

## Known limits

- Text subtitles show Latin, Greek and Cyrillic letters. Chinese, Japanese, Korean, Arabic and Hebrew subtitles do not display: the subtitle fonts do not include them.
- The app always sends the PS4 stereo PCM; Dolby is not passed to the TV as a bitstream. See [AUDIO.md](AUDIO.md).

## Package

`IV0000-JFPS40000_00-JELLYFINPS400000.pkg` - title ID `JFPS40000`, version `01.14`. Installs over 1.13 and keeps your sign-in and settings.
