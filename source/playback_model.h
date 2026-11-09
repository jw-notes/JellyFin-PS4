#pragma once
#include <string>
#include <vector>
std::string select_playback_source(const std::string& json,const std::string& requested_id);
std::string playback_response_summary(const std::string& json);
// A server stream Index as the player numbers its audio tracks: 1 for the
// file's first embedded audio stream. -1 for an external or unknown stream,
// which is not inside the file the player opens.
int playback_audio_ordinal(const std::string& source_json,int stream_index);
// The source's DefaultAudioStreamIndex: the stream the server chose (-1 when absent).
int playback_default_audio(const std::string& source_json);
// A subtitle stream of a source: where it is (inside the file, as the player
// numbers its subtitle tracks, 1 for the first; or a file beside it), its codec,
// and how the server will deliver it ("Embed", "External" with its URL, "Encode").
struct PlaybackSubtitle { bool found=false, external=false; int ordinal=-1; std::string codec, delivery, delivery_url; };
PlaybackSubtitle playback_subtitle(const std::string& source_json,int stream_index);
// One query parameter of a URL, as it is written there ("" when absent).
std::string playback_url_param(const std::string& url,const std::string& name);
// For the log: the container, each video and audio stream, the one selected.
std::vector<std::string> playback_source_report(const std::string& source_json,int selected_audio);
