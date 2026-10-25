#include "keyboard_view.h"
#include "app.h"
#include "platform.h"
#include "art.h"
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
            int label_width=text_width(key.label,24);
            text(x+std::max(6,(w-label_width)/2),y+10,key.label,selected?g_theme.bg:g_theme.text,24,w-12);
            x+=key.units*step;
        }
    }
    draw_hints(60,622,{{'X',"Select"},{'S',"Delete"},{'T',"Caps"},{'O',"Done"},{'C',"Cancel"}});
    draw_hints(60,660,{{'l',""},{'r',"Move text cursor"},{'R',"Letters / symbols"}});
}
