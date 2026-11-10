// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
// Leaving the PS4 app sandbox so USB drives (/mnt/usb0..7) can be written
// (Downloads > USB drive): GoldHEN 2.3+'s own escape through its Plugins SDK (MIT, vendor/goldhen_sdk).
//
// Done once at launch (downloads_startup), on the main thread before other
// threads start: credentials and root directories are process-wide.
namespace sandbox {
// Steps out of the sandbox and drops back to the app's own uid / gid so files
// it writes keep the user's ownership. Logs each step. False if GoldHEN's SDK
// does not answer or the escape fails (the app carries on inside it).
bool leave();
// Hands the original credentials back (at exit).
void restore();
// True while the process is outside the sandbox through leave().
bool lifted();
}
