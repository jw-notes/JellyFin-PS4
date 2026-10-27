#pragma once
// Software updates, the way Apollo Save Tool does them on the PS4: the
// project's latest GitHub release (UPDATE_REPO) is compared with this build;
// a newer one's .pkg is downloaded to /data/pkg, its size and SHA-256 checked,
// and the user installs it from the Package Installer (GoldHEN lists
// /data/pkg there). The app never installs anything itself.
#include <stdint.h>
#include <string>

#define UPDATE_REPO "jw-notes/JellyFin-PS4"
#define UPDATE_DIR "/data/pkg"
#define UPDATE_FILE UPDATE_DIR "/JellyFin-PS4.pkg"

enum UpdateState { UPD_IDLE, UPD_CHECKING, UPD_CURRENT, UPD_AVAILABLE, UPD_FAILED };
// Check in the background (a low-priority thread; it waits `delay_ms` first so
// it never competes with startup). Again from Settings > Software Update.
void update_check_start(int delay_ms=0);
int update_state();
// The newer release's version ("1.11"), once UPD_AVAILABLE.
std::string update_version();

enum DownloadState { DL_IDLE, DL_RUNNING, DL_DONE, DL_FAILED };
void update_download_start();
void update_download_cancel();
int update_download_state();
// Bytes so far and the total (0 until known).
void update_download_progress(uint64_t& got,uint64_t& total);
// Why the check or the download failed, in English (shown through T()).
std::string update_error();
// Join the workers before the network goes away.
void update_shutdown();
