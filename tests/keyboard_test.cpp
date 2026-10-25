#include "keyboard_model.h"
#include "http_diagnostics.h"
#include <cassert>
#include <set>
#include <stdio.h>
static void type(KeyboardEditor& editor,char value){
    for(int attempt=0;attempt<2;attempt++){
        auto rows=editor.layout();
        for(int r=0;r<(int)rows.size();r++)for(int c=0;c<(int)rows[r].size();c++)if(rows[r][c].kind==KeyKind::Character&&rows[r][c].value==std::string(1,value)){
            editor.row=r;editor.column=c;assert(!editor.activate());return;
        }
        editor.toggle_symbols();
    }
    assert(false && "Character missing from keyboard");
}
int main(){
    auto rows=keyboard_layout(false,false,KeyboardType::Text);
    std::string numbers,qwerty,home;
    for(auto& k:rows[0])numbers+=k.value;
    for(auto& k:rows[1])qwerty+=k.value;
    for(auto& k:rows[2])home+=k.value;
    assert(numbers=="1234567890"&&qwerty=="qwertyuiop"&&home=="asdfghjkl");
    std::set<char> available;
    for(bool symbols:{false,true})for(bool caps:{false,true})for(auto& row:keyboard_layout(symbols,caps,KeyboardType::Text))for(auto& key:row)if(key.kind==KeyKind::Character)available.insert(key.value[0]);
    available.insert(' ');for(int c=32;c<127;c++)assert(available.count((char)c));
    KeyboardEditor url("http://",KeyboardType::Url);
    for(char c:std::string("192.168.1.141:8097"))type(url,c);
    assert(url.input=="http://192.168.1.141:8097");
    url.backspace();type(url,'6');assert(url.input=="http://192.168.1.141:8096");url.backspace();type(url,'7');
    rows=url.layout();bool found=false;for(int r=0;r<(int)rows.size();r++)for(int c=0;c<(int)rows[r].size();c++)if(rows[r][c].label=="https://"){url.row=r;url.column=c;url.activate();found=true;}
    assert(found&&url.input=="https://192.168.1.141:8097");
    KeyboardEditor edit("ac");edit.move_cursor(-1);type(edit,'b');assert(edit.input=="abc");edit.backspace();assert(edit.input=="ac");
    KeyboardEditor unicode(u8"a\u00e9b");unicode.move_cursor(-1);unicode.backspace();assert(unicode.input=="ab"&&unicode.cursor==1);
    KeyboardEditor full(std::string(250,'x'));full.insert("a");assert(full.input.size()==250);
    for(int i=0;i<1000;i++){url.move(i%2?1:-1,i%3?1:-1);auto layout=url.layout();assert(url.row>=0&&url.row<(int)layout.size()&&url.column>=0&&url.column<(int)layout[url.row].size());}
    assert(url.displayed(true).find("192.168")==std::string::npos);
    assert(http_log_target("http://user:password@host:8097/Items?api_key=token")=="http://host:8097/Items");
    assert(http_log_target("http://host/QuickConnect/Connect?secret=test")=="http://host/QuickConnect/Connect");
    puts("PASS QWERTY layout, printable characters, complete URL entry, port editing, scheme shortcuts, UTF-8 cursor/delete, length limits, navigation, password masking and diagnostic redaction");
}
