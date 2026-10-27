#pragma once
#include "app.h"
#include "art.h"
#include "platform.h"
#include <string>
#include <vector>
#include "../vendor/ps3/source/ui/render/classic_grid.h"
void app_header(const std::string& title,const std::string& tab="Home",int focused_tab=-1);
// The bottom hint strip, every glyph a controller icon.
void hint_bar(std::initializer_list<Hint> hints);
ClassicGrid library_grid(const Page& page,bool home);
int library_window(const ClassicGrid&,int selected,int first);

// The PS3 letter rail left of a library grid: '#', then A-Z.
constexpr int JUMP_ENTRIES=27;
struct JumpBar { bool show=false, active=false; int sel=1; std::string filter; };
std::string jump_letter(int entry);
int jump_entry(const std::string& name);
void draw_library(const Page& page,const std::string& title,int start,int selected,bool home,int focused_tab=-1,int first=-1,const std::string& breadcrumb="",const JumpBar* jump=nullptr);

// Classic Home (PS3 ui_home_classic.inc): My Media, Continue Watching, Next Up
// and Latest rows under the Home / Search / Settings header.
enum HomeShape { SHAPE_LIBRARY, SHAPE_WIDE, SHAPE_POSTER, SHAPE_SQUARE };
struct HomeRow { std::string title; HomeShape shape=SHAPE_WIDE; Page page; int col=0; float offset=0; };
struct HomeView { std::vector<HomeRow> rows; int focus=0, header=1; float scroll=0; bool loaded=false; };
void home_card_size(HomeShape shape,int& w,int& h);
int home_row_height(const HomeRow& row);
// The artwork a card shows: its cache key and server path.
void home_image(const HomeRow& row,const Item& item,std::string& key,std::string& path);
void home_request_art(const HomeView& home);
void home_step(HomeView& home,bool snap);
void draw_home(const HomeView& home,int header_focus);
// Just Home's header band (the boot handoff redraws it over a still of Home).
void draw_home_header(int header_focus);
// Fetch and size the on-screen cards' artwork, waiting up to timeout_ms.
// Any thread: it only fills the artwork cache.
void home_prepare_art(const HomeView& home,int timeout_ms);
struct PeekOrigin { int x=0,y=0,w=0,h=0; };
PeekOrigin home_focus_rect(const HomeView& home);
PeekOrigin library_focus_rect(const ClassicGrid& g,int selected,int first,bool home);

// Detail page (PS3 Classic ui_info.cpp): backdrop, poster, actions,
// selectors, overview, genres/director and the cast as round photo cards.
enum DetailAction { ACT_RESUME, ACT_PLAY, ACT_START, ACT_FAVOURITE, ACT_WATCHED };
std::vector<DetailAction> detail_actions(const Item& item);
constexpr int DETAIL_CAST_VISIBLE=6;
struct DetailView { std::string version,video,audio,audio_chip,subtitles; int row=0,col=0,cast_first=0; };
void request_detail_art(const Item& item);
void draw_detail(const Item& item,const DetailView& view);

// Player on-screen display (PS3 player HUD v3).
struct PlayerOsd { std::string title,subtitle,audio,subtitles,album,art,up_next; int track=0,tracks=0,viz=0; bool viz_toast=false; uint64_t position=0,duration=0,target=0; bool paused=false,live=false,scrubbing=false,started=false,audio_only=false; };
void draw_player_osd(const PlayerOsd& osd);
// The L1 picture mode's banner: what it is set to, and the buttons.
void draw_picture_banner(const std::string& state,bool moving);
// Now Playing for music: the cover, a visualizer, the artist and album, and
// the progress bar. `bands` are 0..1 levels; `wave` the newest sound (-1..1).
// The visualizer is osd.viz (Square cycles them); osd.viz_toast names it.
enum MusicViz { VIZ_BARS, VIZ_AURORA, VIZ_HALO, VIZ_SCOPE, VIZ_OFF, VIZ_COUNT };
const char* music_viz_name(int viz);
void draw_music_screen(const PlayerOsd& osd,const float* bands,int count,const float* wave,int wave_n);

// `under`: the page beneath, undimmed (the peek dims it in the same pass).
void draw_quick_peek(const Item& item,const PeekOrigin& origin,float progress,const std::string& status="",const uint32_t* under=nullptr);
void draw_search(const std::string& query);
std::string fit_text(const std::string& text,int width,int size=24);
std::string fit_face(const std::string& text,int width,int size,TextFace face);
void paragraph(int x,int y,const std::string& text,int width,int lines,uint32_t color,int size=24,int pitch=29);
std::string clock_text(uint64_t ms);
// The console's local time `after_ms` from now, in its own 12/24 h and date order.
void console_clock(uint64_t after_ms,std::string& time,std::string& date);
// "Ends at 9:42 PM": when something with `remaining_ms` to go finishes.
std::string ends_at(uint64_t remaining_ms);
void draw_star(int x,int y,int size,uint32_t color);

// Settings (PS3 ui_settings.cpp): two levels. First the categories, each a
// tile with its name and what is inside; X opens one, whose rows show their
// value at the right and what the highlighted one does underneath.
enum SettingsIcon { SICON_PLAYBACK, SICON_DISPLAY, SICON_ACCOUNT, SICON_SYSTEM };
struct SettingsCategoryView { std::string label,summary; SettingsIcon icon=SICON_SYSTEM; };
// lit: the value is "on" (drawn in the accent); dim: the row does nothing now;
// action: X opens or does something (a chevron instead of a value).
struct SettingsRowView { std::string label,value,description; bool lit=false,dim=false,action=false; };
void draw_settings_categories(const std::vector<SettingsCategoryView>& categories,int selected,const std::string& account,const std::string& footer);
void draw_settings_rows(const SettingsCategoryView& category,const std::vector<SettingsRowView>& rows,int selected);
// A yes / no question over the current frame.
void draw_confirm(const std::string& question,const std::string& detail,const char* yes);
// Software Update's panel over the current frame: a title, up to two lines of
// detail, a progress bar when progress >= 0, and the X / O hints (null = none).
void draw_update_panel(const std::string& title,const std::string& detail,float progress,const char* yes,const char* no);

// Settings > About (PS3 ui_about.cpp): Updates, Credits and Licences.
int about_sections_count();
int about_max_scroll(int section);
void draw_about(int section,int top);
