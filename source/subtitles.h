// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#pragma once
// Subtitles drawn by the console (PS3 Settings > Subtitles). MPV's libass draws
// text subtitles (SubRip, WebVTT, ASS) in one of three bundled bold faces and
// three looks; PGS and other picture subtitles are drawn as they are. ASS keeps
// its own styling: only a face it names that is not here falls back to the
// chosen one. The server burns subtitles into the picture only when Draw
// Subtitles is set to Server, or when it is converting the video anyway and the
// subtitles are pictures (PGS, DVD).
//
// The faces are the PS3's: Open Sans (the default), Noto Sans and Roboto
// Condensed, all bold. Noto Sans and Roboto Condensed are subset to Latin, and
// the console's libass has no font fallback, so other scripts need Open Sans.
//
// The looks are the PS3's too (vendor/ps3/source/util/subcolor.h): fill,
// outline and outline weight move together.
enum { SUBFONT_OPENSANS, SUBFONT_NOTO, SUBFONT_ROBOCOND, SUBFONT_COUNT };
enum { SUBCOLOUR_WHITE, SUBCOLOUR_YELLOW, SUBCOLOUR_GREY, SUBCOLOUR_COUNT };

inline const char* subtitle_font_label(int font){return font==SUBFONT_NOTO?"Noto Sans":font==SUBFONT_ROBOCOND?"Roboto Cond.":"Open Sans";}
// The family name inside the font file, which is what libass matches.
inline const char* subtitle_font_family(int font){return font==SUBFONT_NOTO?"Noto Sans":font==SUBFONT_ROBOCOND?"Roboto Condensed":"Open Sans";}
inline const char* subtitle_colour_label(int colour){return colour==SUBCOLOUR_YELLOW?"Soft Yellow":colour==SUBCOLOUR_GREY?"Soft Grey":"White";}
// MPV's values: colours as #AARRGGBB, the outline in pixels at 720 lines.
// White and Soft Yellow are opaque with a heavy outline; Soft Grey is slightly
// translucent with a thin one, the quiet option.
struct SubtitleLook { const char* fill; const char* outline; const char* outline_size; };
inline SubtitleLook subtitle_look(int colour){
 if(colour==SUBCOLOUR_YELLOW)return {"#FFFFE79A","#E6000000","3"};
 if(colour==SUBCOLOUR_GREY)return {"#D9E6E8EA","#B4000000","1.5"};
 return {"#FFFFFFFF","#E6000000","3"};
}
