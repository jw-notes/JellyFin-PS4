#pragma once
#include <stdint.h>
#include <string>
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
bool player_open(const std::string& url);
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
uint64_t player_time();
void player_close();
