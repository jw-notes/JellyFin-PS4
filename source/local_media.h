#pragma once
// Media on USB drives, played with no server (PS3 Media tab). The drives are
// read at /mnt/usb0..7, which the app can see once it has left the sandbox at
// launch (downloads_startup). Videos play in the player from their file; a
// folder's music plays as a queue.
#include "app.h"
#include <string>
#include <vector>

enum LocalKind { LOCAL_FOLDER, LOCAL_VIDEO, LOCAL_AUDIO, LOCAL_OTHER };
struct LocalEntry { std::string name, path; LocalKind kind=LOCAL_OTHER; uint64_t bytes=0; };

// The USB slots with a drive in them ("/mnt/usb0"...), in slot order.
std::vector<std::string> usb_drives();
// A cheap fingerprint of which drives are in, to notice one plugged in or out.
std::string usb_drives_signature();
// A folder's sub-folders, videos and music (folders first, then by name;
// hidden and system entries left out). False when it cannot be read.
bool list_local_folder(const std::string& path,std::vector<LocalEntry>& out);
// A folder, and a file's size in bytes (0: missing). stat() is not used on
// the console: its struct does not match the kernel's, so sizes come out wrong.
bool local_is_dir(const std::string& path);
uint64_t local_file_size(const std::string& path);
// What the player and Now Playing need for a file: its name, kind, path.
Item local_item(const LocalEntry& entry);
