# Audio on the PS4

How sound gets from a file to the speakers, what has been confirmed, and what the limits are.

## The path

The app plays through MPV 0.36 with FFmpeg 7.1 in software. `source/player_ps4.cpp` (AvPlayer) is not in the build's source list.

1. The file or stream is demuxed by FFmpeg (Matroska, MP4, MPEG-TS, HLS, raw AC-3 and others).
2. FFmpeg's own decoders produce PCM. `ac3`, `eac3` and `aac` are all linked into the app.
3. MPV's mixer takes anything over two channels to stereo at 48 kHz: left + 0.707 centre + 0.707 left surround, likewise on the right, with no LFE.
4. A limiter (`source/audio_limiter.h`) holds loud passages under full scale. It runs only for sources of more than two channels, and leaves anything below its ceiling unchanged.
5. SDL's PS4 driver sends the PCM to `sceAudioOut`.

## Why Dolby does not use its decoder's downmix

Left to its default, MPV asks the AC-3 / E-AC-3 decoder to mix to stereo. That mix is scaled so it can never clip, which made the same 5.1 mix 6.9 dB quieter in AC-3 than in AAC, and about 6 dB quieter than Jellyfin's own transcode of the same file (RMS, measured on the host's libmpv):

| Same 5.1 mix | Decoder downmix (before) | Mixer (now) |
| --- | --- | --- |
| AAC 5.1 | -25.9 dBFS | -25.9 dBFS |
| AC-3 5.1 | -32.8 dBFS | -25.9 dBFS |
| E-AC-3 5.1 | -32.8 dBFS | -25.9 dBFS |

Going through the mixer means the mix levels carried in a Dolby stream are not used: centre and surrounds are always -3 dB. The mixer can pass full scale (+4.6 dBFS on a real Dolby test file), which is what the limiter is for.

## Passthrough

Dolby is not sent to the TV as a bitstream. The app always outputs PCM, so sound from the TV does not show a Dolby decode happened on the TV.

- `ao_sdl` has no compressed output, and the public `sceAudioOut` port formats in the toolchain are PCM only.
- The toolchain's stubs list `sceAudioOutPtOpen` and `sceAudioOutExPtOpen`. They are undocumented, and nothing here shows what they do or that an app may use them.
- On TV speakers it would add nothing: the result is stereo either way.
- The console's Bitstream (Dolby) audio format setting is a separate matter. User reports suggest the console then encodes its PCM output, which would not be passthrough of the film's track.

## From the server

The server checks audio codecs only when it is sent both a `MediaSourceId` and an `AudioStreamIndex`. Results from Jellyfin 12.2 with the app's profile:

| Asked for | H264 with AAC, AC-3 or E-AC-3 | H264 with DTS or TrueHD |
| --- | --- | --- |
| The default track, no index | Direct play | Direct play |
| A chosen track, with its index | Direct play | Transcode to AAC stereo |

Video the profile refuses (HEVC, for example) is transcoded to H264 with AAC stereo. If the original file cannot be decoded or fails to play, the app should go back once to the same point as a server transcode.

## From a USB drive

Local files have only the player's own decoders, with no server to fall back on. A file whose sound cannot be decoded plays its picture with a message.

USB drives are outside the app's sandbox. The app first asks GoldHEN to lift it (syscall 500). Where that does not answer, it reads and writes the drive through GoldHEN's FTP server on the console, so:

- FTP must be on in GoldHEN's settings.
- A file's speed is limited by that server. About 8 MB/s reading and between 1 and 6 MB/s writing small files were measured over the network from a PC to a USB drive on the console; a very high bitrate film may not keep up.
- Files are read by the app's own FTP reader (`source/local_media.cpp`), handed to the player through mpv's stream API. FFmpeg's `ftp://` gives up seeking back after reaching a file's end, so a file with its index at the end (`tears_of_steel_720p.mov`, a QuickTime file) loaded and then played nothing.
- Downloads saved to the drive go straight to it, with no copy on the console. An interrupted one carries on with `APPE`. `REST` with `STOR` must not be used: on this server it empties the file.
- The server does not report free space or the drive's file system, so a full drive, or FAT32's 4 GB limit, only shows when the write fails. The `.part` stays on the drive until it is deleted from a PC.
- Resume points are kept beside downloads, not beside files only browsed to.

## Track selection

- The track shown on the details page and the one played now agree: the first in the console's language that is not commentary, else the file's default.
- A chosen track direct-plays when the server allows it, instead of always transcoding.
- On a USB drive or a download, Triangle lists the file's audio tracks and switches in place.

## What to read in a log

With Debug Logging on, each playback logs the server's decision, every stream, the selected track, the decoder and output formats, and the port SDL opened. On closing it logs A/V sync, underruns and the output level:

`Audio: output level peak -6.7 dBFS, RMS -25.9 dBFS over 60.0 s; limiter on, input peak -6.7 dBFS, 0 frames limited`
