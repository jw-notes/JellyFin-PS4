#pragma once
// Downloads (PS3 offline downloads): a film, an episode, a whole season or an
// album saved to the console or a USB drive and played without streaming.
//
// Where: the console's /data/JellyFin-PS4/downloads, or <usb>/JellyFin on the
// first connected USB drive when Settings > System > Save Downloads To is
// "USB drive". That is written directly when GoldHEN lifts the app's sandbox at
// startup (sandbox_ps4.h), else through its FTP server, which must be on. Each
// download is the item's original file
// plus <file>.json, the item as the server described it, which is what the
// Downloads page lists.
//
// One file downloads at a time, on a worker; the rest wait in a queue.
#include "app.h"
#include <string>
#include <vector>

struct DownloadEntry { Item item; std::string path; uint64_t bytes=0; bool usb=false; };
struct DownloadStatus {
 bool active=false;          // a file is downloading now
 std::string name, error;    // what is downloading; why the last one failed
 uint64_t got=0, total=0;    // bytes so far / expected (0: the server did not say)
 int queued=0;               // waiting after it
 int finished=0;             // completed this session (the page refreshes on change)
 bool paused=false;          // the queue is paused (Triangle on the Downloads page)
};

// Leave the app sandbox (GoldHEN's own escape) so downloads can be
// written to USB drives. Main thread, at startup, before other threads.
// Remembers the outcome for usb_status().
void downloads_startup();
// Starts what was waiting when the app last closed. Once signed in.
void downloads_resume_queue();
// Pause / resume the queue: the file downloading keeps what it has and
// carries on from there.
void download_pause(bool on);
// A download played to `position_ms`: kept beside the file as its resume
// point (cleared near the end), so it resumes there next time.
void download_played(const Item& item,uint64_t position_ms);
// "" when downloads go to USB fine; otherwise why they go to the console.
std::string usb_status();
// Queue an item. Containers are expanded: a season or series queues its
// episodes, an album its tracks. False (last_error set) when nothing was queued.
bool download_item(const Item& item);
bool download_queued_or_done(const std::string& id);
// One item's download: DLS_NONE, DLS_WAITING (queued), DLS_ACTIVE (progress 0..1,
// or -1 when the size is not known yet) or DLS_DONE.
enum { DLS_NONE, DLS_WAITING, DLS_ACTIVE, DLS_DONE };
int download_state(const std::string& id,float* progress=nullptr);
DownloadStatus download_status();
// Stops the file downloading now (its partial file is removed) and the queue.
void download_cancel();
std::vector<DownloadEntry> downloads_list();
bool download_delete(const DownloadEntry& entry);
void downloads_stop();
