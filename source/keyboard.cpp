#include "keyboard_view.h"
#include "mono_clock.h"
#include "platform.h"
#include <time.h>
#include <unistd.h>
static uint64_t keyboard_millis(){return mono_ms();}
bool keyboard(const std::string& title,std::string& value,bool password,KeyboardType type){
    KeyboardEditor editor(value,type);
    uint32_t previous=buttons();uint64_t repeat_at=0;
    for(;;){
        draw_keyboard(title,editor,password);present();
        uint32_t current=buttons(),pressed=current&~previous;
        constexpr uint32_t repeat_mask=0x10|0x20|0x40|0x80|0x8000|0x400|0x800;
        const auto now=keyboard_millis();
        if(current!=previous)repeat_at=now+350;
        else if((current&repeat_mask)&&now>=repeat_at){pressed|=current&repeat_mask;repeat_at=now+85;}
        previous=current;
        if(pressed&0x2000)return false;
        if(pressed&8){value=editor.input;return true;}
        if(pressed&0x10)editor.move(0,-1);
        if(pressed&0x40)editor.move(0,1);
        if(pressed&0x80)editor.move(-1,0);
        if(pressed&0x20)editor.move(1,0);
        if(pressed&0x400)editor.move_cursor(-1);
        if(pressed&0x800)editor.move_cursor(1);
        if(pressed&0x8000)editor.backspace();
        if(pressed&0x1000)editor.caps=!editor.caps;
        if(pressed&0x200)editor.toggle_symbols();
        if((pressed&0x4000)&&editor.activate()){value=editor.input;return true;}
        usleep(16000);
    }
}
