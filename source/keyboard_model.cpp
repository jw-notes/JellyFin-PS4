#include "keyboard_model.h"
#include <algorithm>
#include <ctype.h>
#include <limits.h>
static KeyboardKey action(KeyKind kind,const char* label,int units) { return {kind,label,"",units}; }
static KeyboardRow characters(const char* value,bool caps=false) {
    KeyboardRow row;
    while(*value){char c=caps?(char)toupper((unsigned char)*value):*value;row.push_back({KeyKind::Character,std::string(1,c),std::string(1,c),1});++value;}
    return row;
}
KeyboardRows keyboard_layout(bool symbols,bool caps,KeyboardType type) {
    KeyboardRows rows;
    rows.push_back(characters("1234567890"));
    if(!symbols){
        rows.push_back(characters("qwertyuiop",caps));
        rows.push_back(characters("asdfghjkl",caps));
        auto bottom=characters("zxcvbnm",caps);
        bottom.insert(bottom.begin(),action(KeyKind::Caps,"Caps",2));
        bottom.push_back(action(KeyKind::Backspace,"Delete",2));
        rows.push_back(bottom);
    }else{
        rows.push_back(characters("!@#$%^&*()"));
        rows.push_back(characters("-_=+[]{}|\\"));
        rows.push_back(characters(":;\"'`~<>?"));
        auto bottom=characters(".,/");bottom.push_back(action(KeyKind::Backspace,"Delete",2));rows.push_back(bottom);
    }
    if(type==KeyboardType::Url){
        KeyboardRow shortcuts={{KeyKind::Scheme,"http://","http://",3},{KeyKind::Scheme,"https://","https://",3}};
        auto punctuation=characters(":/.-_");shortcuts.insert(shortcuts.end(),punctuation.begin(),punctuation.end());rows.push_back(shortcuts);
    }
    rows.push_back({action(KeyKind::Symbols,symbols?"ABC":"Symbols",2),action(KeyKind::Space,"Space",5),action(KeyKind::Done,"Done",3)});
    return rows;
}
KeyboardEditor::KeyboardEditor(const std::string& value,KeyboardType mode):input(value),cursor(value.size()),type(mode){}
KeyboardRows KeyboardEditor::layout()const{return keyboard_layout(symbols,caps,type);}
static int width(const KeyboardRow& row){int units=0;for(auto&key:row)units+=key.units;return units;}
static int center(const KeyboardRow& row,int column){int x=-width(row);for(int i=0;i<column;i++)x+=row[i].units*2;return x+row[column].units;}
void KeyboardEditor::move(int horizontal,int vertical){
    auto rows=layout();
    if(horizontal)column=(column+horizontal+(int)rows[row].size())%(int)rows[row].size();
    if(vertical){int x=center(rows[row],column);row=(row+vertical+(int)rows.size())%(int)rows.size();int best=INT_MAX;for(int i=0;i<(int)rows[row].size();i++){int distance=abs(center(rows[row],i)-x);if(distance<best){best=distance;column=i;}}}
}
void KeyboardEditor::move_cursor(int direction){
    if(direction<0&&cursor){--cursor;while(cursor&&((unsigned char)input[cursor]&0xc0)==0x80)--cursor;}
    if(direction>0&&cursor<input.size()){++cursor;while(cursor<input.size()&&((unsigned char)input[cursor]&0xc0)==0x80)++cursor;}
}
void KeyboardEditor::backspace(){if(!cursor)return;size_t end=cursor;move_cursor(-1);input.erase(cursor,end-cursor);}
void KeyboardEditor::insert(const std::string& value){if(input.size()+value.size()<=250){input.insert(cursor,value);cursor+=value.size();}}
void KeyboardEditor::toggle_symbols(){symbols=!symbols;auto rows=layout();row=std::min(row,(int)rows.size()-1);column=std::min(column,(int)rows[row].size()-1);}
bool KeyboardEditor::activate(){
    auto key=layout()[row][column];
    switch(key.kind){
    case KeyKind::Character:insert(key.value);break;
    case KeyKind::Space:insert(" ");break;
    case KeyKind::Backspace:backspace();break;
    case KeyKind::Caps:caps=!caps;break;
    case KeyKind::Symbols:toggle_symbols();break;
    case KeyKind::Scheme:{size_t old=input.rfind("http://",0)==0?7:input.rfind("https://",0)==0?8:0;if(old){if(input.size()-old+key.value.size()>250)break;input.replace(0,old,key.value);cursor=cursor>=old?cursor-old+key.value.size():key.value.size();}else insert(key.value);break;}
    case KeyKind::Done:return true;
    }
    return false;
}
std::string KeyboardEditor::displayed(bool password)const{
    std::string value=password?std::string(input.size(),'*'):input;
    size_t start=cursor>75?cursor-75:0;
    while(start<value.size()&&((unsigned char)value[start]&0xc0)==0x80)++start;
    std::string shown=value.substr(start,cursor-start)+"|"+value.substr(cursor,25);
    return (start?"...":"")+shown;
}
