#pragma once
#include <stdint.h>
#include <stddef.h>
#include "jellyfin_api.h"
#include "json_lite.h"
#include "theme.h"
#include <string>
#include <vector>
struct Person { std::string id,name,role; bool image=false; };
struct Item { JFItem base{}; std::string overview,cast,genres,rating,collection,director,community,series,series_id,tagline; std::vector<Person> people; int year=0,season=0,episode=0; uint64_t resume=0, duration=0; bool favourite=false, played=false, backdrop=false; };
struct Page { std::vector<Item> items; int total=0; };
// saver: the Artwork screensaver; saver_delay: idle seconds before it starts.
struct Settings { int bitrate=8000000; bool resume=true; bool direct=false; bool saver=true; int saver_delay=180; };
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
bool fetch_home_row(HomeRowKind kind,const Item& library,Page& page);
bool fetch_detail(const Item&, Item&);
bool parse_preview_item(const char* json,Item& result);
bool playback_url(const Item&, int audio, int sub, const std::string& selected_source, std::string& url, std::string& session, std::string& source_id);
void report_playback(const char* event, const Item&, const std::string& session,const std::string& source,uint64_t ms,bool paused);
void end_playback(const std::string& session);
std::string json_quote(const std::string&);
std::string encode(const std::string&);
void plog(const char*);
const char *jf_data_path(const char*);
