// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// The brand lockup, drawn as JellyFin [PS] for PS3 draws it (its ui_widgets.cpp,
// xmb_lockup_geom and xmb_draw_topbar): the mark in the theme's colours, and
// "JELLYFIN" in Archivo Expanded ExtraBold running the theme's wordmark ramp
// over a short extrusion and a drop shadow. The PS3's proportions -- a 21 px
// bell, 14 px type, a 6 px gap, 0.02 em tracking -- scaled to the PS4 header.
#pragma once
#include <stdint.h>
#include <vector>
struct BootHeaderPose;

// The header's lockup: the bell's left edge and height, the line both centre on.
constexpr int LOCKUP_BELL=30,LOCKUP_BELL_X=51,LOCKUP_CY=30;
constexpr int LOCKUP_WORD_PX=LOCKUP_BELL*14/21;           // 20
constexpr int LOCKUP_GAP=(LOCKUP_BELL*6+10)/21;          // 9
// The mark raster: 88 viewBox units square, of which the bell is 72, inset 8.
constexpr float MARK_SPAN_U=88,MARK_BELL_U=72,MARK_PAD_U=8;

// Which drawn mark the theme gets: the cool one (Jellywave's purple to blue),
// the gold one (Golden Age), or the cool one recoloured to the theme's pair.
enum MarkVariant { MARK_COOL, MARK_GOLD, MARK_TINT };
MarkVariant mark_variant();
// The theme's pair, to notice a change of theme.
uint64_t mark_theme_key();
// Recolour the cool mark's RGBA in place to the theme's lk_mark_a -> lk_mark_b.
void mark_tint(unsigned char* rgba,int pixels);
// The 80 px mark for the theme, as straight RGBA.
bool mark_rgba(std::vector<uint8_t>& rgba,int& w,int& h);

// The lockup in the header; a pose while the boot animation assembles it.
void draw_lockup(const BootHeaderPose* pose);
