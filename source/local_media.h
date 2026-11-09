#pragma once
// Media on USB drives, played with no server (PS3 Media tab). The drives are
// read at /mnt/usb0..7, which the app can see once it has left the sandbox at
// launch (downloads_startup); when it could not, through GoldHEN's FTP server,
// under the same paths. Videos play in the player from their file; a folder's
// music plays as a queue.
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
// The player's URL for a file read through GoldHEN's FTP server: on the console,
// not out on the network.
bool local_is_drive_url(const std::string& url);
// A path that has to go through that server, and its URL and back.
bool local_via_ftp(const std::string& path);
std::string local_file_url(const std::string& path);
std::string local_url_path(const std::string& url);
// The player's reader for such a file, as mpv's stream API asks: a read of 0 is
// the file's end, below 0 a failure. Open gives null when it cannot.
void* ftp_reader_open(const std::string& path);
int64_t ftp_reader_read(void* reader,char* into,uint64_t room);
int64_t ftp_reader_seek(void* reader,uint64_t to);
int64_t ftp_reader_size(void* reader);
void ftp_reader_cancel(void* reader);
void ftp_reader_close(void* reader);
// Writing to the drive through that server (downloads). An upload starts a file
// or adds to its end. Close says whether the server confirmed it; pass finished
// false for one given up on, which only needs the connection closed.
struct FtpUpload;
FtpUpload* ftp_upload_open(const std::string& path,bool append);
bool ftp_upload_write(FtpUpload* upload,const char* bytes,size_t count);
bool ftp_upload_close(FtpUpload* upload,bool finished);
bool ftp_make_folder(const std::string& path);
uint64_t ftp_file_size(const std::string& path);
bool ftp_delete(const std::string& path);
bool ftp_rename(const std::string& from,const std::string& to);
bool ftp_read_text(const std::string& path,std::string& out);
bool ftp_write_text(const std::string& path,const std::string& text);
// A folder's files (not sub-folders), hidden ones and all, with their sizes.
bool ftp_folder_files(const std::string& path,std::vector<LocalEntry>& out);
// Subtitle files beside a video, named after it ("Film.en.srt", "Film.srt"):
// what the player can open each with, and a name for the menu ("English",
// "Chinese (traditional)"). `video` is a path or the player's URL for one.
struct LocalSubtitleFile { std::string url, label, lang, tag; };
std::vector<LocalSubtitleFile> local_subtitle_files(const std::string& video);
// While a hold is on (playback), the FTP drive watch makes no connections.
void usb_poll_hold(bool on);
