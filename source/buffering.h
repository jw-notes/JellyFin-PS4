#pragma once
// The PS3 playback buffering screen (vendor/ps3/source/ui/render/
// ui_buffering.cpp) on the PS4's software renderer. Its motion is the PS3's
// own buffer_anim.h, unchanged; this module turns its frames into pixels.
//
// Back to front: the item's artwork full screen (the detail page's backdrop,
// else its poster) under a dark veil, or with no artwork a soft pool of the
// accent; a breathing glow; the ring's faint track and its arc in Jellyfin's
// purple -> blue; the Jellyfin mark. Then PREPARING / CONNECTING / BUFFERING
// and the percentage under the ring, the title at the lower left and the
// Circle hint. When playback is ready the ring closes, the mark pulses once
// and everything fades before the first frame.
#include "app.h"
#include <functional>
#include <string>

// Start a presentation for this item (its id picks the cached artwork).
void buffering_begin(const Item& item,const std::string& title);
// The eyebrow ("PREPARING", "CONNECTING", "BUFFERING"), whether the
// percentage shows, and the Circle hint's label (null for none).
void buffering_step(const char* label,bool show_pct,const char* circle_hint);
// Real progress 0..1; a lower value than before is ignored.
void buffering_progress(float p);
// Draw one frame into the framebuffer; the caller presents it.
void buffering_frame();
// Run `work` on a worker thread, presenting frames until it returns.
void buffering_run(const std::function<void()>& work);
// End it: ready closes the ring, pulses and fades; otherwise a quick fade.
// Presents its own frames and returns on a black screen.
void buffering_finish(bool ready);
bool buffering_active();
