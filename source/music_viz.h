#pragma once
// The PS3 music visualizer's spectrum bands (vendor/ps3/source/music/
// music_fft.cpp) on the PS4. The PS3 decoder pushed its own PCM; here MPV
// plays through SDL, so the build links with --wrap=SDL_OpenAudio and the
// wrapper puts a tap in front of MPV's audio callback: every buffer SDL
// takes is copied, as mono, into a ring the UI reads once per frame.
//
// Each frame music_viz_bands() runs a Hann window and a 2048-point FFT over
// the newest samples, folds the magnitudes into MUSIC_VIZ_BANDS log-spaced
// bands (40 Hz - 16 kHz), normalises them to the frame's loudest and eases
// them: a fast attack, a slow fall.
constexpr int MUSIC_VIZ_BANDS=28;

// Clear the tap and the smoothing (call when music starts).
void music_viz_reset();
// Push n interleaved stereo float pairs. Thread-safe against music_viz_bands.
void music_viz_push(const float* lr,int n_pairs);
// out[MUSIC_VIZ_BANDS], 0..1. With no fresh audio (paused, stalled) the bars
// fall smoothly to the floor.
void music_viz_bands(float* out);
// The newest stretch of sound itself (about 21 ms), as n points from -1 to 1,
// for the Scope visualizer. Zeros when there is no fresh audio.
void music_viz_wave(float* out,int n);
