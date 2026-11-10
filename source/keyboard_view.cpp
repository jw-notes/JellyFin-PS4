// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#include "keyboard_view.h"
#include "app.h"
#include "platform.h"
#include "art.h"
#include "language.h"
#include <algorithm>
void draw_keyboard(const std::string& title,const KeyboardEditor& editor,bool password){
    clear(g_theme.bg);
    text(60,35,title,g_theme.text,32);
    rect(60,96,1160,60,g_theme.panel);rect(60,153,1160,3,g_theme.accent);
    text(78,113,editor.displayed(password),g_theme.accent,24,1120);
    auto rows=editor.layout();
    constexpr int step=94,gap=8,key_height=46;
    for(int r=0;r<(int)rows.size();r++){
        int units=0;for(const auto&key:rows[r])units+=key.units;
        int x=(WIDTH-(units*step-gap))/2,y=196+r*56;
        for(int c=0;c<(int)rows[r].size();c++){
            auto key=rows[r][c];int w=key.units*step-gap;bool selected=r==editor.row&&c==editor.column;
            uint32_t bg=selected?g_theme.accent:key.kind==KeyKind::Caps&&editor.caps?g_theme.accent_deep:g_theme.panel_hi;
            rect(x,y,w,key_height,bg);
            // Action keys (Caps, Delete, Space, Done...) are words to translate; characters are not.
            const std::string label=key.kind==KeyKind::Character||key.kind==KeyKind::Scheme?key.label:std::string(T(key.label.c_str()));
            int label_width=text_width(label,24);
            text(x+std::max(6,(w-label_width)/2),y+10,label,selected?g_theme.bg:g_theme.text,24,w-12);
            x+=key.units*step;
        }
    }
    draw_hints(60,622,{{'X',"Select"},{'S',"Delete"},{'T',"Caps"},{'O',"Done"},{'C',"Cancel"}});
    draw_hints(60,660,{{'l',""},{'r',"Move text cursor"},{'R',"Letters / symbols"}});
}
// The same keys, sized to a panel (Search draws them beside its results).
int draw_keyboard_panel(const KeyboardEditor& editor,int x,int y,int w,bool focused){
    auto rows=editor.layout();int widest=1;
    for(const auto& row:rows){int units=0;for(const auto& key:row)units+=key.units;widest=std::max(widest,units);}
    const int step=w/widest,gap=5,key_height=38,pitch=44;
    for(int r=0;r<(int)rows.size();r++){
        int units=0;for(const auto& key:rows[r])units+=key.units;
        int kx=x+(w-(units*step-gap))/2;const int ky=y+r*pitch;
        for(int c=0;c<(int)rows[r].size();c++){
            const auto& key=rows[r][c];const int kw=key.units*step-gap;const bool here=r==editor.row&&c==editor.column,selected=here&&focused;
            const uint32_t bg=selected?g_theme.accent:key.kind==KeyKind::Caps&&editor.caps?g_theme.accent_deep:g_theme.panel_hi;
            rect(kx,ky,kw,key_height,bg);if(here&&!focused){rect(kx,ky,kw,1,g_theme.text_faint);rect(kx,ky+key_height-1,kw,1,g_theme.text_faint);rect(kx,ky,1,key_height,g_theme.text_faint);rect(kx+kw-1,ky,1,key_height,g_theme.text_faint);}
            const std::string label=key.kind==KeyKind::Character||key.kind==KeyKind::Scheme?key.label:std::string(T(key.label.c_str()));
            const int size=key.kind==KeyKind::Character?20:15,lw=text_width(label,size);
            text(kx+std::max(4,(kw-lw)/2),ky+(key_height-size)/2-2,label,selected?g_theme.bg:g_theme.text,size,kw-8);
            kx+=key.units*step;
        }
    }
    return (int)rows.size()*pitch;
}
