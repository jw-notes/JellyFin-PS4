#pragma once
#include <stdint.h>
#include <stddef.h>
#include "jellyfin_api.h"
#include "json_lite.h"
#include "theme.h"
#include <string>
#include <vector>
struct Person { std::string id,name,role; bool image=false; };
struct Item { JFItem base{}; std::string overview,cast,genres,rating,collection,director,community,series,series_id,tagline,album,album_id,artist; std::vector<Person> people; int year=0,season=0,episode=0; uint64_t resume=0, duration=0; bool favourite=false, played=false, backdrop=false; };
struct Page { std::vector<Item> items; int total=0; };
// saver: the Artwork screensaver; saver_delay: idle seconds before it starts.
// language: "auto" (follow the console) or a code such as "ja" (language.h).
// hd1080: ask the server for 1920x1080 instead of 1280x720 (experimental).
// method: how a film or song reaches the player (PLAY_*).
enum PlayMethod { PLAY_AUTO, PLAY_DIRECT_FILE, PLAY_TRANSCODE };
struct Settings { int bitrate=20000000; bool resume=true; int method=PLAY_AUTO; bool saver=true; int saver_delay=180; std::string language="auto"; bool hd1080=true;
 // debug_log: write the diagnostic log. pic_*: the L1 picture mode (picture_size.h).
 bool debug_log=false; int pic_aspect=0,pic_w=100,pic_h=100,pic_x=0,pic_y=0;
 // viz: the music visualizer (MusicViz in ui_view.h; Square cycles it).
 int viz=0; };
extern Settings settings;
extern std::string device_id, data_dir, last_error;
bool storage_init(int user);
bool save_settings();
bool load_session();
bool save_session();
bool login(const std::string& user,const std::string& password);
void logout();
void app_shutdown();
std::string http_failure_detail();
// Stream a GET to a file (see http_ps4.cpp); returns the HTTP status or -1.
#include <functional>
int http_download_file(const char* url,const char* path,const std::function<bool(uint64_t,uint64_t)>& progress);
int http_context_id();
int http_ssl_context_id();
bool fetch_page(const std::string& parent, int start, const std::string& query, int mode, Page& page, const std::string& letter="");
// Library pages are cached; neighbouring pages are fetched ahead in the background.
void prefetch_page(const std::string& parent,int start,const std::string& query,int mode,const std::string& letter="");
bool page_ready(const std::string& parent,int start,const std::string& query,int mode,const std::string& letter="");
void page_cache_clear();
void page_cache_stop();
// Classic Home rows, as on the PS3: Continue Watching, Next Up and Latest <library>.
enum HomeRowKind { HOME_CONTINUE, HOME_NEXTUP, HOME_LATEST };
// Thread-safe: its own buffer, and last_error is left alone.
bool fetch_home_row(HomeRowKind kind,const Item& library,Page& page);
// An album's tracks in disc and track order.
bool fetch_album_tracks(const std::string& album_id,Page& page);
bool fetch_detail(const Item&, Item&);
bool parse_preview_item(const char* json,Item& result);
bool playback_url(const Item&, int audio, int sub, const std::string& selected_source, std::string& url, std::string& session, std::string& source_id);
// Whether the last playback_url() is the original file (the server sends it
// as it is: positions are absolute) rather than a transcode from the resume point.
bool playback_is_static();
// Whether the last playback_url() is a Live TV channel's own stream, played
// straight from its provider rather than through the server.
bool playback_is_remote();
// The next playback_url() goes through the server even where a channel could
// be played directly (after a direct stream failed to start).
void playback_force_server_once();
// Stopped report, transcode clean-up and live stream close, on a thread of
// their own: leaving the player does not wait for the server.
void finish_playback(const Item& item,const std::string& session,const std::string& source,uint64_t ms,bool paused);
void report_playback(const char* event, const Item&, const std::string& session,const std::string& source,uint64_t ms,bool paused);
void end_playback(const std::string& session);
std::string json_quote(const std::string&);
std::string encode(const std::string&);
void plog(const char*);
const char *jf_data_path(const char*);
