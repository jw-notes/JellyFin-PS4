#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "keyboard_model.h"
constexpr int WIDTH=1280, HEIGHT=720;
void render_init();
bool platform_init();
void platform_shutdown();
[[noreturn]] void platform_exit(int status);
uint32_t buttons();
uint32_t* framebuffer();
void present();
void clear(uint32_t color);
void rect(int x,int y,int w,int h,uint32_t color);
// Straight alpha over the current frame (0 transparent .. 255 opaque).
void blend_rect(int x,int y,int w,int h,uint32_t color,int alpha);
int text_width(const std::string&,int size=24);
enum class TextFace { Default, Classic, Poster };
int face_text_width(const std::string&,int size,TextFace face);
void face_text(int x,int y,const std::string&,uint32_t color,int size,TextFace face,int maxwidth=1180);
void text(int x,int y,const std::string&,uint32_t color,int size=24,int maxwidth=1180);
void wrapped(int x,int y,const std::string&,uint32_t color,int lines=5);
bool keyboard(const std::string& title,std::string& value,bool password=false,KeyboardType type=KeyboardType::Text);
int current_user();
// "free memory 812 MB": logged during playback to catch leaks.
std::string resource_report();
// The audio track the next player_open() starts on, as the player numbers them
// (1 for the file's first); 0 lets it choose.
void player_prefer_audio(int track);
bool player_open(const std::string& url);
// The audio tracks of the file playing, and switching among them in place.
struct PlayerAudioTrack { int id=0; std::string label; bool selected=false; };
// The subtitles the next player_open() shows: track -1 as MPV chooses (a file
// with no server), 0 none, else that subtitle track of the file; or a subtitle
// file at `url`, shown `delay_ms` later than its own times say.
void player_prefer_subtitle(int track,const std::string& url,int64_t delay_ms);
// The file's subtitle tracks (PlayerAudioTrack's fields), and a switch in place (0: off).
std::vector<PlayerAudioTrack> player_subtitle_tracks();
void player_select_subtitle(int track);
// Subtitle files beside the next file player_open() plays (a USB drive's
// "Film.en.srt"). player_subtitle_tracks() lists them after the file's own, with
// ids below 0; one is loaded only when chosen.
struct PlayerSubtitleFile { std::string url, label, lang; };
void player_subtitle_files(const std::vector<PlayerSubtitleFile>& files);
std::vector<PlayerAudioTrack> player_audio_tracks();
void player_select_audio(int track);
// The file has an audio track but it could not be decoded or output: picture only.
bool player_audio_failed();
bool player_draw();
bool player_active();
bool player_has_audio();
// The stream has opened (FFmpeg has its format); seconds read ahead of the decoder.
bool player_loaded();
// mono_ms() of the newest video frame (0 before the first).
uint64_t player_last_frame_ms();
// Load `url` again in the open player (Live TV's reconnect), not a new one.
bool player_reload(const std::string& url);
double player_cache_seconds();
// The picture: aspect 0 Fit, 1 Fill, 2 Stretch; width / height in percent of
// the fitted picture; x / y shift in half-percents of the screen (+ right, up).
void player_picture(int aspect,int w,int h,int x,int y);
// The volume, 0..100.
void player_volume(int volume);
std::string player_error();
void player_pause(bool);
void player_seek(uint64_t ms);
// The length the player found in the file, in ms (0 until it knows, or live).
uint64_t player_duration_ms();
uint64_t player_time();
void player_close();
