#pragma once
// The PS3 Artwork screensaver (vendor/ps3/source/ui/xmb/ui_ambient.cpp,
// Artwork mode only): after the idle delay the screen dims away and Home's
// items show as full-screen backdrops, each held 14 s and crossfading to the
// next, with the row, title, episode, year / rating / runtime and synopsis at
// the bottom left. Any button brings the interface back; that press does
// nothing else. Settings > Screensaver turns it on or off and sets the delay.
#include "app.h"
#include <string>
#include <vector>

// The slides: Home's Continue Watching, Next Up and Latest items, labelled
// with their row. Called whenever Home's rows load.
struct SaverItem { Item item; std::string row; };
void screensaver_set_items(const std::vector<SaverItem>& items);
// Called from ui_input() on every input sample. Runs the screensaver modally
// when the idle delay has passed; returns true when it ran (the caller drops
// the sample).
bool screensaver_poll(uint32_t raw_buttons,uint64_t now_ms);
// Playback holds the screensaver off while it is in scope.
struct ScreensaverInhibit { ScreensaverInhibit(); ~ScreensaverInhibit(); };
// Settings labels and steps.
std::string screensaver_delay_label();
void screensaver_delay_step(int dir);
