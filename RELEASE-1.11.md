# Jellyfin [PS] for PS4 — 1.11

## New

- **TV shows play on** to the next episode, across seasons. **L1 / R1** skip episodes, and the player bar shows what's up next.
- **Skip Intros / Skip Credits** (Settings › Playback): Off, Button or Auto. Intros are skipped in films and shows; in shows the credits offer **Skip to Next Episode**. Needs intro / credits markers on the server (Jellyfin 10.10+ with a plugin such as Intro Skipper); without them both settings read "Not on server".
- **Player bar, as on the PS3**: Left / Right scrub; Up moves to Play / Pause, **Volume** and **Picture**, and X uses what's highlighted. The volume is remembered. In Picture, Circle closes it and holding Triangle for 3 seconds resets it.
- **Live TV**
  - **L1 / R1 change channel**; Up / Down set the volume; no pausing or scrubbing.
  - **A guide on the PS4** (Settings › Live TV): Update Guide, Auto-Update Guide and Guide Address (an XMLTV address, or the playlist's own `url-tvg`). Any user can update it. The bar shows what's on now, the time left and what's next.
  - Recovers by itself when a channel's picture stops (Pluto ad breaks), with an animated "Reconnecting".
- **Quick peek shows the cast**, with photos.
- **Home Order** (Settings › Display): Live TV, Films, Shows, then the rest by default, or your own order.
- **Share Sign-in** (Settings › Account): lend your account to this console's users who have no sign-in of their own.
- **The controller's light bar** takes the theme's colour.

## Fixed

- Live TV no longer crashes or freezes when a channel reconnects.
- Albums whose tracks are in disc folders (CD1, CD2) no longer show empty.
- Films no longer appear twice when scrolling a large library.
- Themes cycle through every theme once, in order.
- The controller is picked up again after it turns off or disconnects.
- Resume Playback is on by default (switched on once for installs updating from an earlier version).

## Package

`IV0000-JFPS40000_00-JELLYFINPS400000.pkg` — title ID `JFPS40000`, version `01.11`. Installs over 1.10 and keeps your sign-in and settings; Settings › Software Update offers it on 1.10.
