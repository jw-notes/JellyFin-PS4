#pragma once
// The Live TV guide, downloaded to the PS4 (Settings › Live TV): an XMLTV file
// from the address the viewer gave, else the one the playlist names
// (#EXTM3U url-tvg="..."), found once through Jellyfin's tuner setup when an
// admin signs in and remembered for everyone. Any user can update it; Auto
// updates it at launch when it is over 12 hours old.
//
// Only what the app shows is kept -- each channel's shows from an hour ago
// to a day and a half ahead -- in guide.tsv, read back at the next launch.
// Channels are matched by name (Jellyfin does not pass the playlist's tvg-id
// to apps), ignoring case, punctuation and tags such as "(720p)".
#include <string>
#include <time.h>

enum GuideState { GUIDE_IDLE, GUIDE_UPDATING, GUIDE_FAILED };
// Load the saved guide; with Auto on, update it in the background when due.
// Call after sign-in.
void guide_start();
// Update now, in the background (no-op while one runs).
void guide_update_now();
int guide_state();
// For Settings: "Updated 2 h ago · 84 channels · 1,920 shows", or why not.
std::string guide_status();
// The address in use ("" when there is none yet) and where it came from.
std::string guide_address(bool* from_playlist=nullptr);

struct GuideNow { std::string now,next; time_t now_start=0,now_stop=0,next_start=0; };
// What is on `channel_name` now and next; false when the guide has nothing.
bool guide_now_next(const std::string& channel_name,GuideNow& out);
