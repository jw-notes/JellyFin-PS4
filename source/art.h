#pragma once
#include <string>
#include <initializer_list>
void load_art(const std::string& id);
// Any server image under its own cache key, e.g. a backdrop or a cast photo.
void load_image(const std::string& key,const std::string& path);
// 0 not loaded yet, 1 ready, -1 the server had no usable image.
int art_state(const std::string& key);
// 0..1: how far a just-arrived image has faded in (1 for anything older).
float art_fade(const std::string& key);
// Changes whenever new artwork arrives: a cheap "anything new to draw?".
unsigned art_generation();
// Scale a cached image for a card of this size now, so drawing it later is a
// plain copy. Any thread; false if the image is not here.
bool art_prepare(const std::string& key,int w,int h,bool cover=true);
// keep=false for a size drawn once (a full-screen backdrop being composed,
// an animating panel): it is scaled for this call without being cached.
bool draw_art(const std::string& id,int x,int y,int w,int h,bool keep=true,int alpha=255);
bool draw_art_cover(const std::string& id,int x,int y,int w,int h,bool keep=true,int alpha=255);
// Fractional position and size, bilinear: for anything animating.
bool draw_art_smooth(const std::string& id,float x,float y,float w,float h,bool cover=false,int alpha=255);
bool draw_art_circle(const std::string& id,int x,int y,int d);
// alpha 0..255 fades the mark over what is already drawn.
void draw_brand_mark(int x,int y,int size,int alpha=255);

// Controller glyphs: X C S T (face buttons), D (d-pad), l r (L1 R1),
// L R (L2 R2), O (Options) and P (touchpad).
int draw_button_icon(char glyph,int x,int y,int height);
struct Hint{char glyph;const char* label;};
int draw_hints(int x,int y,std::initializer_list<Hint> hints,int size=18);
int hints_width(std::initializer_list<Hint> hints,int size=18);

void art_wait();
void art_stop(bool clear_cache=true);
