#pragma once
#include <string>
#include <initializer_list>
void load_art(const std::string& id);
// Any server image under its own cache key, e.g. a backdrop or a cast photo.
void load_image(const std::string& key,const std::string& path);
// 0 not loaded yet, 1 ready, -1 the server had no usable image.
int art_state(const std::string& key);
// keep=false for a size drawn once (a full-screen backdrop being composed,
// an animating panel): it is scaled for this call without being cached.
bool draw_art(const std::string& id,int x,int y,int w,int h,bool keep=true);
bool draw_art_cover(const std::string& id,int x,int y,int w,int h,bool keep=true);
bool draw_art_circle(const std::string& id,int x,int y,int d);
void draw_brand_mark(int x,int y,int size);

// Controller glyphs: X C S T (face buttons), D (d-pad), l r (L1 R1),
// L R (L2 R2), O (Options) and P (touchpad).
int draw_button_icon(char glyph,int x,int y,int height);
struct Hint{char glyph;const char* label;};
int draw_hints(int x,int y,std::initializer_list<Hint> hints,int size=18);
int hints_width(std::initializer_list<Hint> hints,int size=18);

void art_wait();
void art_stop(bool clear_cache=true);
