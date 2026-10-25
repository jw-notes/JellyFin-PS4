#pragma once
// The PS3 cold-boot animation on the PS4's software renderer. The timeline is
// the PS3's own vendor/ps3/source/ui/render/boot_seq.h, unchanged; this module
// turns its frames into pixels.
//
//   DARK / EMBLEM / AWAIT   black, the centred mark fading in, a breathing halo
//                           (boot_pump while startup work runs)
//   SHINE                   a glint sweeps the mark and a star twinkles on it
//   EMERGE / DOCK           Home rises out of the black while the mark glides
//                           into the header's brand slot (boot_overlay)
//   WORDMARK                "Jellyfin" unfolds out of the docked mark
//
// "0" in <data>/bootanim.txt turns it off.
#include <stdint.h>
#include <functional>

// The header's brand mark and wordmark while the animation poses them.
struct BootHeaderPose { bool show_mark; float word_reveal[8], word_alpha[8], slide_px; };

void boot_begin();
bool boot_active();
// One frame of the pre-Home sequence over black, presented.
void boot_pump();
// Run `work` on a worker thread, pumping frames until it returns.
void boot_run(const std::function<void()>& work);
// Startup is going to sign-in or an error screen: fade out to black.
void boot_leave();
// Home has been drawn: start the handoff.
void boot_ready();
// Per Home frame: advance the clock before drawing...
void boot_tick();
// True while the animation owns the whole screen: skip drawing Home.
bool boot_covers_home();
// ...and draw the veil and the travelling mark over the finished frame.
void boot_overlay();
// A button press during the handoff skips to the end.
void boot_skip();
// Null when the header draws normally.
const BootHeaderPose* boot_header_pose();
