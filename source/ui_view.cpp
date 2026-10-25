#include "ui_view.h"
#include "platform.h"
#include "art.h"
#include "boot_anim.h"
#include <algorithm>
#include <math.h>
#include <string.h>
#include "../vendor/ps3/source/ui/render/peek.h"
static uint32_t mix(uint32_t a,uint32_t b,int t){int r=(((a>>16)&255)*(255-t)+((b>>16)&255)*t)/255,g=(((a>>8)&255)*(255-t)+((b>>8)&255)*t)/255,bl=((a&255)*(255-t)+(b&255)*t)/255;return (r<<16)|(g<<8)|bl;}
static void outline(int x,int y,int w,int h,uint32_t color,int thickness=2){rect(x,y,w,thickness,color);rect(x,y+h-thickness,w,thickness,color);rect(x,y,thickness,h,color);rect(x+w-thickness,y,thickness,h,color);}
// Rounded rectangle: an optional fill, then an optional one-pixel edge.
static void round_rect(int x,int y,int w,int h,int r,uint32_t fill,int fill_a,uint32_t line=0,int line_a=0){
 if(w<=0||h<=0)return;r=std::min(r,std::min(w,h)/2);
 auto inset=[&](int row){if(row<0||row>=h)return w/2;float dy=row<r?r-row-0.5f:row>=h-r?row-(h-r)+0.5f:0.0f;return dy>0?(int)(r-sqrtf(std::max(0.0f,(float)r*r-dy*dy))+0.5f):0;};
 for(int row=0;row<h;++row){int in=inset(row),left=x+in,width=w-2*in;if(width<=0)continue;
  if(fill_a)blend_rect(left,y+row,width,1,fill,fill_a);
  if(!line_a)continue;
  if(row==0||row==h-1){blend_rect(left,y+row,width,1,line,line_a);continue;}
  int span=std::max(1,std::min(w/2,std::max(inset(row-1),inset(row+1)))-in);
  blend_rect(left,y+row,span,1,line,line_a);blend_rect(x+w-in-span,y+row,span,1,line,line_a);}
}
static void focus_round(int x,int y,int w,int h,int r){round_rect(x-3,y-3,w+6,h+6,r+3,0,0,g_theme.focus_ring,255);round_rect(x-2,y-2,w+4,h+4,r+2,0,0,g_theme.focus_ring,255);}
static void circle(int cx,int cy,int radius,uint32_t color,int alpha){for(int dy=-radius;dy<=radius;++dy){int half=(int)sqrtf((float)(radius*radius-dy*dy));blend_rect(cx-half,cy+dy,2*half+1,1,color,alpha);}}
static void ring(int cx,int cy,int radius,uint32_t color){for(int a=0;a<360;++a){float t=a*3.14159265f/180;int px=cx+(int)lroundf(radius*cosf(t)),py=cy+(int)lroundf(radius*sinf(t));rect(px,py,1,1,color);}}
void draw_star(int x,int y,int size,uint32_t color){
 // A filled five-point star: inside when the angle-relative radius fits.
 float c=size/2.0f,outer=c,inner=c*0.45f;
 for(int row=0;row<size;++row)for(int col=0;col<size;++col){float dx=col+0.5f-c,dy=row+0.5f-c-size*0.04f,d=sqrtf(dx*dx+dy*dy);if(d>outer)continue;float a=atan2f(dx,-dy);if(a<0)a+=6.2831853f;float sector=fmodf(a,1.2566371f)/1.2566371f;float edge=sector<0.5f?outer+(inner-outer)*sector*2:inner+(outer-inner)*(sector-0.5f)*2;if(d<=edge)rect(x+col,y+row,1,1,color);}
}
std::string fit_text(const std::string& input,int width,int size){if(text_width(input,size)<=width)return input;std::string value=input;while(!value.empty()&&text_width(value+"...",size)>width){size_t end=value.size()-1;while(end&&((unsigned char)value[end]&0xc0)==0x80)--end;value.resize(end);}return value+"...";}
void paragraph(int x,int y,const std::string& input,int width,int lines,uint32_t color,int size,int pitch){
 size_t at=0;for(int line=0;line<lines&&at<input.size();++line){size_t end=at,last_space=at;while(end<input.size()){size_t next=end+1;while(next<input.size()&&((unsigned char)input[next]&0xc0)==0x80)++next;if(input[end]=='\n')break;if(text_width(input.substr(at,next-at),size)>width)break;if(input[end]==' ')last_space=end;end=next;}if(end==at&&(at>=input.size()||input[at]!='\n'))break;if(end<input.size()&&input[end]!='\n'&&last_space>at)end=last_space;std::string value=input.substr(at,end-at);if(line==lines-1&&end<input.size())value=fit_text(value+"...",width,size);text(x,y+line*pitch,value,color,size,width);at=end;while(at<input.size()&&(input[at]==' '||input[at]=='\n'||input[at]=='\r'))++at;}
}
static std::string fit_face(const std::string& value,int width,int size,TextFace face){if(face_text_width(value,size,face)<=width)return value;std::string s=value;while(!s.empty()&&face_text_width(s+"...",size,face)>width){size_t end=s.size()-1;while(end&&((unsigned char)s[end]&0xc0)==0x80)--end;s.resize(end);}return s+"...";}
std::string clock_text(uint64_t ms){unsigned s=(unsigned)(ms/1000);char b[24];if(s>=3600)snprintf(b,sizeof b,"%u:%02u:%02u",s/3600,s/60%60,s%60);else snprintf(b,sizeof b,"%u:%02u",s/60,s%60);return b;}
static std::string runtime_text(uint64_t ms){unsigned m=(unsigned)((ms+59999)/60000);char b[24];if(m>=60)snprintf(b,sizeof b,"%uh %um",m/60,m%60);else snprintf(b,sizeof b,"%um",m);return b;}
static void background(int y0,int y1){for(int y=std::max(0,y0);y<std::min(HEIGHT,y1);++y)rect(0,y,WIDTH,1,mix(g_theme.bg_top,g_theme.bg_bot,y*255/(HEIGHT-1)));}
static const char* const header_tabs[]={"Home","Search","Settings"};
static void header_chrome(const std::string& tab,int focused_tab){
 if(const auto* pose=boot_header_pose()){
  // The boot animation docks the mark here, then unfolds the word out of it.
  if(pose->show_mark)draw_brand_mark(48,12,36);
  const char* word="Jellyfin";int x=96;
  for(int i=0;i<8;++i){std::string letter(1,word[i]);int a=(int)(255*std::max(0.0f,std::min(1.0f,pose->word_alpha[i])));if(a>0)face_text(x-(int)((1-pose->word_reveal[i])*pose->slide_px),17,letter,mix(g_theme.bg_top,g_theme.text,a),20,TextFace::Classic);x+=face_text_width(letter,20,TextFace::Classic);}
  int a=(int)(255*std::max(0.0f,std::min(1.0f,pose->word_alpha[7])));if(a>0){face_text(x,17," [PS]",mix(g_theme.bg_top,g_theme.text,a),20,TextFace::Classic);face_text(240,20,"PS4",mix(g_theme.bg_top,g_theme.accent,a),15,TextFace::Classic);}
 }
 else{draw_brand_mark(48,12,36);face_text(96,17,"Jellyfin [PS]",g_theme.text,20,TextFace::Classic);face_text(240,20,"PS4",g_theme.accent,15,TextFace::Classic);}
 int widths[3],total=20;for(int i=0;i<3;++i){widths[i]=face_text_width(header_tabs[i],17,TextFace::Classic)+36;total+=widths[i];}int x=(WIDTH-total)/2;
 for(int i=0;i<3;++i){bool focused=i==focused_tab,active=tab==header_tabs[i];if(!tab.empty()){if(focused)rect(x,14,widths[i],34,g_theme.accent);else if(active)rect(x+14,45,widths[i]-28,3,g_theme.accent);face_text(x+18,20,header_tabs[i],focused?g_theme.bg:active?g_theme.text:g_theme.text_dim,17,TextFace::Classic);}x+=widths[i]+10;}
}
void app_header(const std::string& title,const std::string& tab,int focused_tab){
 background(0,HEIGHT);header_chrome(tab,focused_tab);
 if(!title.empty())face_text(64,86,fit_face(title,1100,24,TextFace::Classic),g_theme.text,24,TextFace::Classic);
}
void hint_bar(std::initializer_list<Hint> hints){rect(0,650,WIDTH,70,g_theme.bg_bot);rect(0,650,WIDTH,1,mix(g_theme.bg_bot,g_theme.text_faint,60));draw_hints(64,674,hints);}
static std::string kind(const Item& item){const char* type=item.base.type;if(!strcmp(type,"CollectionFolder")||!strcmp(type,"UserView"))return "Library";if(!strcmp(type,"MusicAlbum"))return "Album";if(!strcmp(type,"MusicArtist"))return "Artist";if(!strcmp(type,"TvChannel"))return "Live TV";if(!strcmp(type,"Navigation"))return "Browse";return type;}
static std::string episode_code(const Item& item){if(strcmp(item.base.type,"Episode")||!item.episode)return {};char b[32];snprintf(b,sizeof b,"S%d:E%d",item.season,item.episode);return b;}

// ---- library grid --------------------------------------------------------
ClassicGrid library_grid(const Page& page,bool home){
 if(home)return {1,4,281,158,230,1184,48};
 const char* type=page.items.empty()?"Movie":page.items.front().base.type;
 bool square=!strcmp(type,"MusicAlbum")||!strcmp(type,"Audio")||!strcmp(type,"MusicArtist")||!strcmp(type,"Playlist");
 bool portrait=!square&&strcmp(type,"Episode")&&strcmp(type,"Video")&&strcmp(type,"TvChannel");
 return classic_grid_make(WIDTH,HEIGHT,64,156,70,square?68:50,24,10,portrait,square);
}
int library_window(const ClassicGrid& g,int selected,int first){int row=std::max(0,first/g.cols),focus=std::max(0,selected/g.cols);if(focus<row)row=focus;if(focus>=row+g.rows)row=focus-g.rows+1;return row*g.cols;}
PeekOrigin library_focus_rect(const ClassicGrid& g,int selected,int first,bool home){int relative=selected-first;return {g.x0+(relative%g.cols)*(g.card_w+(home?20:24)),(home?174:156)+(relative/g.cols)*g.stride,g.card_w,g.card_h};}
std::string jump_letter(int entry){return entry<=0?"#":std::string(1,(char)('A'+std::min(25,entry-1)));}
int jump_entry(const std::string& name){
 // Skip leading articles and punctuation the way the server's SortName does not;
 // only the first character decides, as on the PS3.
 if(name.empty())return 0;char c=name[0];if(c>='a'&&c<='z')c=(char)(c-32);return c>='A'&&c<='Z'?1+(c-'A'):0;
}
static void draw_jump_bar(const JumpBar& jump,int x,int top,int bottom,int current){
 // A window of fifteen letters centred on the current one, fading to its ends.
 constexpr int window=15,half=window/2;int pitch=std::min(30,(bottom-top)/window);
 int centre=std::max(half,std::min(JUMP_ENTRIES-1-half,current));
 if(jump.active)round_rect(x-6,top-4,32,window*pitch+8,8,g_theme.panel,150,g_theme.hairline,120);
 for(int i=centre-half;i<=centre+half;++i){int d=std::abs(i-centre);bool selected=jump.active&&i==jump.sel,here=!jump.active&&i==current;
  uint32_t base=selected?g_theme.accent_alt:here?g_theme.text:g_theme.text_dim;uint32_t color=mix(base,g_theme.bg,std::min(220,d*d*200/(half*half)));
  int y=top+(i-(centre-half))*pitch;std::string label=jump_letter(i);int size=selected||here?19:16;
  if(selected){rect(x-2,y+2,24,pitch-4,g_theme.accent);color=g_theme.bg;}
  face_text(x+(20-face_text_width(label,size,TextFace::Classic))/2,y+(pitch-size)/2,label,color,size,TextFace::Classic);}
}
void draw_library(const Page& page,const std::string& title,int start,int selected,bool home,int focused_tab,int first,const std::string& breadcrumb,const JumpBar* jump){
 const auto g=library_grid(page,home);first=library_window(g,selected,std::max(0,first));int top=home?174:156,gap=home?20:24,visible=g.cols*g.rows;
 app_header(home?"My Media":"",title.rfind("Search:",0)==0?"Search":"Home",focused_tab);
 if(!home){face_text(g.x0,86,fit_face(title,900,24,TextFace::Classic),g_theme.text,24,TextFace::Classic);
  std::string sub=breadcrumb;if(sub.empty()&&jump&&!jump->filter.empty())sub=jump->filter=="#"?"Names before A":"Starting with "+jump->filter;
  if(!sub.empty())face_text(g.x0,122,fit_face(sub,g.grid_w,15,TextFace::Classic),g_theme.text_dim,15,TextFace::Classic);}
 int last=std::min(first+visible,(int)page.items.size());std::string count=page.items.empty()?"No items":std::to_string(start+first+1)+" - "+std::to_string(start+last)+" / "+std::to_string(page.total);
 if(home)text(48,143,count,g_theme.text_dim);else face_text(WIDTH-g.x0-face_text_width(count,14,TextFace::Classic),95,count,g_theme.text_dim,14,TextFace::Classic);
 for(int i=first;i<last;++i){const auto& item=page.items[i];int relative=i-first,x=g.x0+(relative%g.cols)*(g.card_w+gap),y=top+(relative/g.cols)*g.stride;bool focus=i==selected&&!(jump&&jump->active);
  rect(x,y,g.card_w,g.card_h,g_theme.panel);bool image=home?draw_art_cover(item.base.id,x,y,g.card_w,g.card_h):draw_art(item.base.id,x,y,g.card_w,g.card_h);
  if(!image)face_text(x+10,y+g.card_h/2-12,fit_face(item.base.name,g.card_w-20,18,TextFace::Classic),g_theme.text_dim,18,TextFace::Classic,g.card_w-20);
  if(focus)outline(x-3,y-3,g.card_w+6,g.card_h+6,g_theme.focus_ring,2);
  if(home){text(x,y+g.card_h+12,fit_text(item.base.name,g.card_w),focus?g_theme.text:g_theme.text_dim,24,g.card_w);text(x,y+g.card_h+42,kind(item),g_theme.text_faint,24,g.card_w);}
  else{bool wide=g.card_w>g.card_h;int size=wide?(focus?20:16):(focus?21:20);TextFace face=wide?TextFace::Classic:TextFace::Poster;face_text(x,y+g.card_h+8,fit_face(item.base.name,g.card_w,size,face),focus?g_theme.text:g_theme.text_dim,size,face,g.card_w);
   if(focus){std::string meta=item.year?std::to_string(item.year):"";if(item.duration){if(!meta.empty())meta+="  ";meta+=std::to_string(item.duration/60000)+" min";}face_text(x,y+g.card_h+34,fit_face(meta,g.card_w,14,TextFace::Poster),g_theme.text_dim,14,TextFace::Poster,g.card_w);}}
  if(item.duration&&item.resume){int fill=(int)(g.card_w*std::min(1.0,(double)item.resume/item.duration));rect(x,y+g.card_h-3,g.card_w,3,g_theme.track);rect(x,y+g.card_h-3,fill,3,g_theme.accent);}
 }
 if(!home&&page.items.empty()&&jump&&!jump->filter.empty())face_text(g.x0,top+36,"Nothing starts with "+jump->filter+". Press Left to pick another letter.",g_theme.text_dim,18,TextFace::Classic);
 if(!home&&jump&&jump->show){int current=jump->active?jump->sel:!jump->filter.empty()?jump_entry(jump->filter=="#"?"":jump->filter):page.items.empty()?1:jump_entry(page.items[std::min(selected,(int)page.items.size()-1)].base.name);draw_jump_bar(*jump,std::max(8,g.x0-46),top,640,current);}
 if(home&&!page.items.empty()){const auto& item=page.items[selected];text(48,430,fit_text(item.base.name,1130,32),g_theme.text,32);paragraph(48,478,item.overview.empty()?(is_container(item.base.type)?"Open this library to browse your media.":"Open for details, versions and playback options."):item.overview,1150,4,g_theme.text_dim);}
 if(!home&&page.total>visible){int bar_h=(g.rows-1)*g.stride+g.card_h,thumb=std::max(24,bar_h*visible/page.total),offset=(bar_h-thumb)*(start+first)/std::max(1,page.total-visible);rect(g.x0+g.grid_w+14,top,3,bar_h,g_theme.track);rect(g.x0+g.grid_w+14,top+std::min(bar_h-thumb,offset),3,thumb,g_theme.accent);}
 if(jump&&jump->active)hint_bar({{'D',"Letter"},{'X',"Jump"},{'C',"Cancel"}});
 else if(focused_tab>=0)hint_bar({{'D',"Nav"},{'X',"Select"},{'C',"Back"}});
 else if(jump&&jump->show)hint_bar({{'D',"Nav"},{'X',"Open"},{'T',"Details"},{'C',jump->filter.empty()?"Back":"Clear letter"},{'l',""},{'r',"Tabs"}});
 else hint_bar({{'D',"Nav"},{'X',"Open"},{'T',"Details"},{'C',"Back"},{'l',""},{'r',"Tabs"}});
 if(jump&&jump->show&&!jump->active){const char* label="Left at first column: A-Z";int x=WIDTH-64-hints_width({{'D',label}});draw_hints(x,674,{{'D',label}});}
}

// ---- classic home --------------------------------------------------------
constexpr int HOME_X0=64,HOME_TOP=96,HOME_GAP=18,HOME_TITLE_H=34,HOME_LABEL_H=48,HOME_ROW_GAP=12,HOME_CLIP=88,HOME_BOTTOM=650;
void home_card_size(HomeShape shape,int& w,int& h){if(shape==SHAPE_POSTER){w=140;h=210;}else if(shape==SHAPE_SQUARE){w=168;h=168;}else{w=256;h=144;}}
int home_row_height(const HomeRow& row){int w,h;home_card_size(row.shape,w,h);return HOME_TITLE_H+h+HOME_LABEL_H+HOME_ROW_GAP;}
static int home_step_px(const HomeRow& row){int w,h;home_card_size(row.shape,w,h);return w+HOME_GAP;}
static int home_row_y(const HomeView& home,int index){int y=HOME_TOP;for(int k=0;k<index;++k)y+=home_row_height(home.rows[k]);return y-(int)lroundf(home.scroll);}
void home_image(const HomeRow& row,const Item& item,std::string& key,std::string& path){
 key=item.base.id;path="/Items/"+encode(item.base.id)+"/Images/Primary?MaxWidth=320&Quality=85";
 // Wide rows show a film's backdrop; an episode's own image is already a still.
 if(row.shape==SHAPE_WIDE&&strcmp(item.base.type,"Episode")&&item.backdrop){key+=":wide";path="/Items/"+encode(item.base.id)+"/Images/Backdrop?MaxWidth=480&Quality=80";}
}
void home_request_art(const HomeView& home){
 for(size_t i=0;i<home.rows.size();++i){const auto& row=home.rows[i];int y=home_row_y(home,(int)i);if(y>HEIGHT||y+home_row_height(row)<0)continue;int step=home_step_px(row);
  for(size_t c=0;c<row.page.items.size();++c){int x=HOME_X0+(int)c*step-(int)row.offset;if(x>WIDTH+step||x<-2*step)continue;std::string key,path;home_image(row,row.page.items[c],key,path);load_image(key,path);}}
}
void home_step(HomeView& home,bool snap){
 // The focused row's title sits at the top of the content area; each row
 // keeps one card visible left of its focus once it has moved on.
 float target=0;for(int k=0;k<std::max(0,home.focus);++k)target+=home_row_height(home.rows[k]);
 home.scroll=snap?target:home.scroll+(target-home.scroll)*0.35f;if(fabsf(home.scroll-target)<0.5f)home.scroll=target;
 const int avail=WIDTH-HOME_X0-32;
 for(auto& row:home.rows){int n=(int)row.page.items.size();row.col=std::max(0,std::min(row.col,n-1));int step=home_step_px(row);float t=(float)(std::max(0,row.col-1)*step),max_off=(float)(n*step-avail);t=std::max(0.0f,std::min(t,max_off));row.offset=snap?t:row.offset+(t-row.offset)*0.35f;if(fabsf(row.offset-t)<0.5f)row.offset=t;}
}
PeekOrigin home_focus_rect(const HomeView& home){
 if(home.focus<0||home.focus>=(int)home.rows.size())return {};const auto& row=home.rows[home.focus];int w,h;home_card_size(row.shape,w,h);
 return {HOME_X0+row.col*home_step_px(row)-(int)lroundf(row.offset),home_row_y(home,home.focus)+HOME_TITLE_H,w,h};
}
static void home_labels(const HomeRow& row,const Item& item,std::string& a,std::string& b){
 a=item.base.name;b.clear();
 if(row.shape==SHAPE_LIBRARY)return;
 if(!strcmp(item.base.type,"Episode")){if(!item.series.empty())a=item.series;b=episode_code(item);b+=(b.empty()?"":" - ")+std::string(item.base.name);}
 else if(item.year)b=std::to_string(item.year);
}
void draw_home(const HomeView& home,int header_focus){
 background(0,HEIGHT);
 for(size_t i=0;i<home.rows.size();++i){
  const auto& row=home.rows[i];const int y0=home_row_y(home,(int)i);if(y0>HOME_BOTTOM||y0+home_row_height(row)<HOME_CLIP)continue;
  int w,h;home_card_size(row.shape,w,h);const int cy=y0+HOME_TITLE_H,step=home_step_px(row);const bool row_focus=(int)i==home.focus;
  face_text(HOME_X0,y0,row.title,row_focus?g_theme.text:g_theme.text_dim,20,TextFace::Classic);
  for(size_t c=0;c<row.page.items.size();++c){
   const int cx=HOME_X0+(int)c*step-(int)lroundf(row.offset);if(cx>WIDTH||cx+w<0)continue;const auto& item=row.page.items[c];
   rect(cx,cy,w,h,g_theme.thumb_dim);std::string key,path;home_image(row,item,key,path);
   bool image=draw_art_cover(key,cx,cy,w,h);if(!image&&key!=item.base.id)image=draw_art_cover(item.base.id,cx,cy,w,h);
   if(!image){if(row.shape==SHAPE_LIBRARY)draw_brand_mark(cx+(w-48)/2,cy+(h-48)/2,48);else face_text(cx+10,cy+h/2-10,fit_face(item.base.name,w-20,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic,w-20);if(art_state(item.base.id)==0)load_art(item.base.id);}
   if(item.duration&&item.resume&&row.shape!=SHAPE_LIBRARY){int fill=(int)(w*std::min(1.0,(double)item.resume/item.duration));rect(cx,cy+h-4,w,4,g_theme.track);rect(cx,cy+h-4,fill,4,g_theme.accent_alt);}
   const bool on=row_focus&&(int)c==row.col&&home.focus>=0;if(on)outline(cx-3,cy-3,w+6,h+6,g_theme.focus_ring,2);
   std::string a,b;home_labels(row,item,a,b);const int ly=cy+h+8;
   face_text(cx,ly,fit_face(a,w,16,TextFace::Classic),on?g_theme.white:g_theme.text,16,TextFace::Classic,w);
   if(!b.empty())face_text(cx,ly+21,fit_face(b,w,14,TextFace::Classic),g_theme.text_faint,14,TextFace::Classic,w);
  }
 }
 // Content scrolls under the header: repaint the band it passes beneath.
 background(0,HOME_CLIP);header_chrome("Home",header_focus);
 if(home.rows.empty())face_text(HOME_X0,HOME_TOP+36,home.loaded?"Nothing to show yet. Add a library on your Jellyfin server.":"Loading...",g_theme.text_dim,18,TextFace::Classic);
 if(header_focus>=0)hint_bar({{'D',"Nav"},{'X',"Select"},{'l',""},{'r',"Tabs"}});
 else if(home.focus>=0&&home.focus<(int)home.rows.size()&&home.rows[home.focus].shape==SHAPE_LIBRARY)hint_bar({{'D',"Nav"},{'X',"Open"},{'S',"Favourites"},{'l',""},{'r',"Tabs"}});
 else hint_bar({{'D',"Nav"},{'X',"Open"},{'T',"Details"},{'S',"Favourites"},{'l',""},{'r',"Tabs"}});
}

// ---- search ----------------------------------------------------------------
void draw_search(const std::string& query){
 app_header("Search your library","Search");
 rect(48,190,1184,88,g_theme.panel_hi);outline(48,190,1184,88,g_theme.focus_ring,2);
 text(72,219,fit_text(query.empty()?"Enter a title, show, artist or album":query,1120,32),query.empty()?g_theme.text_dim:g_theme.text,32,1120);
 text(48,330,"Search across your Jellyfin libraries.",g_theme.text_dim);
 hint_bar({{'X',"Enter search"},{'C',"Back"},{'l',"Home"},{'r',"Settings"}});
}

// ---- quick peek ----------------------------------------------------------------
void draw_quick_peek(const Item& item,const PeekOrigin& origin,float progress,const std::string& status){
 const peek_rect from={(float)origin.x,(float)origin.y,(float)origin.w,(float)origin.h};
 const auto frame=peek_eval(progress,from,peek_panel(WIDTH/2.0f,HEIGHT/2.0f,520));
 int dim=(int)(frame.lift*170);blend_rect(0,0,WIDTH,HEIGHT,0,dim);
 const int x=(int)frame.r.x,y=(int)frame.r.y,w=(int)frame.r.w,h=(int)frame.r.h;
 rect(x,y,w,h,g_theme.panel_hi);outline(x,y,w,h,g_theme.focus_ring,2);
 if(!frame.back){draw_art(item.base.id,x,y,w,h,progress>=1.0f);return;}
 if(frame.content_a<0.01f)return;
 const auto ink=mix(g_theme.panel_hi,g_theme.text,(int)(255*frame.content_a));
 const auto muted=mix(g_theme.panel_hi,g_theme.text_dim,(int)(255*frame.content_a));
 const int tx=x+210,tw=w-234;
 draw_art(item.base.id,x+24,y+24,160,240);
 text(tx,y+26,fit_text(item.base.name,tw,32),ink,32,tw);
 std::string metadata=kind(item);if(!episode_code(item).empty())metadata+="  "+episode_code(item);if(item.year)metadata+="  "+std::to_string(item.year);if(item.duration)metadata+="  "+runtime_text(item.duration);
 text(tx,y+73,fit_text(metadata,tw),muted,24,tw);
 text(tx,y+108,fit_text(item.rating+(item.rating.empty()?"":"  ")+item.genres,tw),muted,24,tw);
 paragraph(tx,y+151,item.overview.empty()?(status.empty()?"No synopsis available.":status):item.overview,tw,9,ink);
 if(!item.cast.empty()){text(x+24,y+285,"Cast",muted);paragraph(x+24,y+320,item.cast,160,4,ink);}
 draw_hints(x+24,y+h-42,{{'X',is_container(item.base.type)?"Open":"Details"},{'T',"Close"},{'C',"Close"}},20);
}

// ---- detail page -----------------------------------------------------------------
std::vector<DetailAction> detail_actions(const Item& item){
 std::vector<DetailAction> actions;
 if(item.resume&&settings.resume){actions.push_back(ACT_RESUME);actions.push_back(ACT_START);}else actions.push_back(ACT_PLAY);
 actions.push_back(ACT_FAVOURITE);actions.push_back(ACT_WATCHED);return actions;
}
static std::vector<uint32_t> composed;static std::string composed_key;
static std::string backdrop_key(const Item& item){return std::string(item.base.id)+":backdrop";}
void request_detail_art(const Item& item){
 composed_key.clear();load_art(item.base.id);
 if(item.backdrop)load_image(backdrop_key(item),"/Items/"+encode(item.base.id)+"/Images/Backdrop?MaxWidth=1280&Quality=80");
 else if(!item.series_id.empty())load_image(backdrop_key(item),"/Items/"+encode(item.series_id)+"/Images/Backdrop?MaxWidth=1280&Quality=80");
 for(const auto& person:item.people)if(person.image&&!person.id.empty())load_image("person:"+person.id,"/Items/"+encode(person.id)+"/Images/Primary?MaxWidth=200&Quality=85");
}
// The backdrop and its scrims change only when the image arrives: compose
// them once and copy the result into later frames.
static void detail_background(const Item& item){
 const auto key=backdrop_key(item);
 if(composed_key==key&&composed.size()==(size_t)WIDTH*HEIGHT){memcpy(framebuffer(),composed.data(),composed.size()*4);return;}
 background(0,HEIGHT);const bool art=draw_art_cover(key,0,0,WIDTH,HEIGHT,false);
 // Left-to-right scrim (dark behind the text, the artwork through on the
 // right) and a bottom scrim under the cast, as PS3 Classic draws them.
 static int column[WIDTH];for(int x=0;x<WIDTH;++x){float t=(float)x/(WIDTH-1);column[x]=art?(t<0.46f?(int)(238-(238-190)*t/0.46f):(int)(190-(190-30)*(t-0.46f)/0.54f)):70;}
 auto pixels=framebuffer();
 for(int y=0;y<HEIGHT;++y){int bottom=y<450?0:std::min(242,(y-450)*242/200);for(int x=0;x<WIDTH;++x){int a=255-(255-column[x])*(255-bottom)/255;uint32_t d=pixels[y*WIDTH+x];int keep=255-a;pixels[y*WIDTH+x]=0xff000000|(((((d>>16)&255)*keep+3*a)/255)<<16)|(((((d>>8)&255)*keep+4*a)/255)<<8)|(((d&255)*keep+8*a)/255);}}
 if(art||art_state(key)<0||(!item.backdrop&&item.series_id.empty())){composed.assign(pixels,pixels+WIDTH*HEIGHT);composed_key=key;}
}
static std::string action_label(DetailAction id,const Item& item){
 if(id==ACT_RESUME)return "Resume "+clock_text(item.resume);if(id==ACT_PLAY)return "Play";if(id==ACT_START)return "Play from start";
 if(id==ACT_FAVOURITE)return item.favourite?"Favourited":"Favourite";return item.played?"Watched":"Mark as watched";
}
// A person with no photo: the default silhouette in the photo's circle.
static void avatar(int x,int y,int d){
 const int r=d/2,cx=x+r,cy=y+r;circle(cx,cy,r,g_theme.thumb_dim,255);circle(cx,y+r*4/5,r*7/20,g_theme.text_faint,200);
 const int br=r*31/50,bcy=y+d+r/5;
 for(int dy=-br;dy<=0;++dy){int yy=bcy+dy;if(yy>=y+d)continue;int half=(int)sqrtf((float)(br*br-dy*dy)),oy=yy-cy,limit=(int)sqrtf((float)std::max(0,r*r-oy*oy));half=std::min(half,limit);blend_rect(cx-half,yy,2*half,1,g_theme.text_faint,200);}
}
void draw_detail(const Item& item,const DetailView& view){
 detail_background(item);header_chrome("Home",-1);
 constexpr int PX=57,PY=76,PW=288,PH=432,TX=PX+PW+40,TW=WIDTH-TX-57;
 // Poster with a hairline frame.
 rect(PX,PY,PW,PH,g_theme.thumb_dim);if(!draw_art_cover(item.base.id,PX,PY,PW,PH))face_text(PX+16,PY+PH/2-12,fit_face(item.base.name,PW-32,20,TextFace::Classic),g_theme.text_dim,20,TextFace::Classic);outline(PX-1,PY-1,PW+2,PH+2,g_theme.hairline,1);
 // Title (series above it for an episode).
 int y=72;if(!strcmp(item.base.type,"Episode")&&!item.series.empty()){face_text(TX,y,fit_face(item.series,TW,17,TextFace::Classic),g_theme.text_dim,17,TextFace::Classic);y+=22;}
 face_text(TX,y,fit_face(item.base.name,TW,34,TextFace::Classic),g_theme.text,34,TextFace::Classic);
 // Metadata: year · runtime · certificate · SxEy, then the score row.
 std::vector<std::string> parts;if(!episode_code(item).empty())parts.push_back(episode_code(item));if(item.year)parts.push_back(std::to_string(item.year));if(item.duration)parts.push_back(runtime_text(item.duration));if(!item.rating.empty())parts.push_back(item.rating);
 int mx=TX;const int my=y+50;for(size_t i=0;i<parts.size();++i){if(i){face_text(mx+6,my,"\xC2\xB7",g_theme.text_faint,17,TextFace::Classic);mx+=20;}face_text(mx,my,parts[i],g_theme.text_dim,17,TextFace::Classic);mx+=face_text_width(parts[i],17,TextFace::Classic);}
 int sx=TX;const int sy=my+27;
 if(!item.community.empty()){draw_star(sx,sy+1,17,g_theme.accent_alt);sx+=24;face_text(sx,sy,item.community+"/10",g_theme.accent_alt,17,TextFace::Classic);sx+=face_text_width(item.community+"/10",17,TextFace::Classic)+24;}
 if(item.played){face_text(sx,sy,"Watched",g_theme.text_dim,17,TextFace::Classic);sx+=face_text_width("Watched",17,TextFace::Classic)+24;}
 if(item.duration&&item.resume&&settings.resume){std::string left=runtime_text(item.duration>item.resume?item.duration-item.resume:0)+" left";face_text(sx,sy,left,g_theme.text_dim,17,TextFace::Classic);}
 // Chips: the selected version's video summary and its audio track.
 int cx=TX;const int chip_y=sy+28;
 for(const auto& chip:{view.video,view.audio_chip}){if(chip.empty())continue;int w=face_text_width(chip,15,TextFace::Classic)+22;if(cx+w>TX+TW)break;round_rect(cx,chip_y,w,29,14,g_theme.panel,220,g_theme.hairline,255);face_text(cx+11,chip_y+6,chip,g_theme.text_dim,15,TextFace::Classic);cx+=w+8;}
 // Action buttons.
 const auto actions=detail_actions(item);int ax=TX;const int AY=chip_y+50,AH=44;
 for(size_t i=0;i<actions.size();++i){const auto id=actions[i];const bool primary=id==ACT_RESUME||id==ACT_PLAY;const std::string label=action_label(id,item);
  int w=face_text_width(label,17,TextFace::Classic)+(primary?72:id==ACT_FAVOURITE?66:40);w=std::max(w,primary?150:120);
  if(primary)round_rect(ax,AY,w,AH,5,g_theme.accent,235,g_theme.accent_alt,230);else round_rect(ax,AY,w,AH,5,g_theme.panel,160,g_theme.hairline,255);
  int lx=ax+(w-face_text_width(label,17,TextFace::Classic))/2;
  if(primary){int g=draw_button_icon('X',ax+18,AY+(AH-24)/2,24);lx=ax+18+g+12;}
  if(id==ACT_FAVOURITE){int gx=ax+(w-(26+face_text_width(label,17,TextFace::Classic)))/2;draw_star(gx,AY+(AH-18)/2,18,item.favourite?g_theme.accent_alt:g_theme.text_dim);lx=gx+26;}
  face_text(lx,AY+(AH-17)/2-1,label,primary?g_theme.white:g_theme.text,17,TextFace::Classic);
  if(view.row==0&&view.col==(int)i)focus_round(ax,AY,w,AH,5);ax+=w+12;}
 // Selectors: version, audio, subtitles.
 const std::string names[]={"Version","Audio","Subtitles"},values[]={view.version,view.audio.empty()?"Server default":view.audio,view.subtitles};
 int selx=TX;const int SY=AY+AH+14,SH=36;
 for(int i=0;i<3;++i){int nw=face_text_width(names[i],14,TextFace::Classic);std::string value=fit_face(values[i],330-nw-40,16,TextFace::Classic);int w=std::min(330,nw+12+face_text_width(value,16,TextFace::Classic)+30);if(selx+w>TX+TW)w=TX+TW-selx;if(w<80)break;
  round_rect(selx,SY,w,SH,5,g_theme.panel,230,g_theme.hairline,255);face_text(selx+15,SY+10,names[i],g_theme.text_faint,14,TextFace::Classic);face_text(selx+15+nw+12,SY+9,fit_face(value,w-nw-42,16,TextFace::Classic),view.row==1&&view.col==i?g_theme.white:g_theme.text,16,TextFace::Classic);
  if(view.row==1&&view.col==i)focus_round(selx,SY,w,SH,5);selx+=w+12;}
 // Overview, then labelled rows as on Jellyfin's web page.
 int oy=SY+SH+18;std::string overview=item.overview;if(overview.empty()&&!item.tagline.empty())overview=item.tagline;
 if(!overview.empty()){paragraph(TX,oy,overview,TW,item.people.empty()?6:3,g_theme.text_dim,17,23);oy+=(item.people.empty()?6:3)*23+8;}
 const int vx=TX+100;
 if(!item.genres.empty()){face_text(TX,oy,"Genres",g_theme.text_faint,15,TextFace::Classic);face_text(vx,oy,fit_face(item.genres,TX+TW-vx,15,TextFace::Classic),g_theme.text,15,TextFace::Classic);oy+=22;}
 if(!item.director.empty()){face_text(TX,oy,"Director",g_theme.text_faint,15,TextFace::Classic);face_text(vx,oy,fit_face(item.director,TX+TW-vx,15,TextFace::Classic),g_theme.text,15,TextFace::Classic);oy+=22;}
 // Cast: round photos with names centred under them, a window that follows the focus.
 if(!item.people.empty()){
  constexpr int KD=92,KP=130;const int KY=std::max(oy+8,486);const int total=(int)item.people.size();
  for(int k=0;k<DETAIL_CAST_VISIBLE&&view.cast_first+k<total;++k){const auto& person=item.people[view.cast_first+k];const int kx=TX+k*KP,px=kx+(KP-KD)/2;
   bool photo=person.image&&draw_art_circle("person:"+person.id,px,KY,KD);if(!photo)avatar(px,KY,KD);ring(px+KD/2,KY+KD/2,KD/2,g_theme.hairline);
   const bool focus=view.row==2&&view.cast_first+k==view.col;if(focus){ring(px+KD/2,KY+KD/2,KD/2+3,g_theme.focus_ring);ring(px+KD/2,KY+KD/2,KD/2+4,g_theme.focus_ring);ring(px+KD/2,KY+KD/2,KD/2+2,g_theme.focus_ring);}
   std::string name=fit_face(person.name,KP-8,15,TextFace::Classic),role=fit_face(person.role,KP-8,13,TextFace::Classic);
   face_text(kx+(KP-face_text_width(name,15,TextFace::Classic))/2,KY+KD+7,name,focus?g_theme.white:g_theme.text,15,TextFace::Classic);
   if(!role.empty())face_text(kx+(KP-face_text_width(role,13,TextFace::Classic))/2,KY+KD+27,role,g_theme.text_dim,13,TextFace::Classic);}
  if(view.cast_first>0)text(TX-24,KY+KD/2-16,"\xE2\x80\xB9",g_theme.text_dim,28);
  if(view.cast_first+DETAIL_CAST_VISIBLE<total)text(TX+DETAIL_CAST_VISIBLE*KP+4,KY+KD/2-16,"\xE2\x80\xBA",g_theme.text_dim,28);
 }
 if(view.row==2)hint_bar({{'D',"Nav"},{'X',"Filmography"},{'C',"Back"}});
 else if(view.row==1)hint_bar({{'D',"Nav"},{'X',"Change"},{'C',"Back"}});
 else hint_bar({{'D',"Nav"},{'X',"Select"},{'C',"Back"}});
}

// ---- player OSD ------------------------------------------------------------------
void draw_player_osd(const PlayerOsd& osd){
 // Bottom scrim: transparent at y=459 to black 0.90 at 62% of its height.
 const int y0=459,h=HEIGHT-y0;for(int i=0;i<h;i+=2){int a=std::min(230,(int)(230.0f*i/(h*0.62f)));blend_rect(0,y0+i,WIDTH,2,0,a);}
 // Play / pause disc and the title beside it.
 const int d=50,bx=57,by=529;round_rect(bx,by,d,d,d/2,g_theme.accent,190,g_theme.white,70);
 const int cx=bx+d/2,cy=by+d/2;
 if(osd.paused||!osd.started){const int th=d*26/64,tw=th*7/8,tx=cx-tw/2+d/32,ty=cy-th/2;for(int r=0;r<th;++r){float f=(float)r/(th-1);int w=(int)((1.0f-fabsf(2.0f*f-1.0f))*tw+0.5f);if(w>0)rect(tx,ty+r,w,1,g_theme.white);}}
 else{const int bh=d*24/64,bw=d*7/64,gap=d*7/64;rect(cx-gap/2-bw,cy-bh/2,bw,bh,g_theme.white);rect(cx+gap/2,cy-bh/2,bw,bh,g_theme.white);}
 const int tx0=bx+d+20;face_text(tx0,by+(osd.subtitle.empty()?9:0),fit_face(osd.title,820,30,TextFace::Classic),g_theme.white,30,TextFace::Classic);
 if(!osd.subtitle.empty())face_text(tx0,by+36,fit_face(osd.subtitle,820,15,TextFace::Classic),0xC2C2C2,15,TextFace::Classic);
 // Read-only chips at the right: the playing audio and subtitle tracks.
 int x=1223;const int chip_y=by+6;
 const std::string keys[]={"SUBTITLES","AUDIO"},values[]={osd.subtitles.empty()?"Off":osd.subtitles,osd.audio.empty()?"Default":osd.audio};
 for(int i=0;i<2&&!osd.audio_only;++i){std::string value=fit_face(values[i],240,15,TextFace::Classic);int w=std::max(face_text_width(keys[i],11,TextFace::Classic),face_text_width(value,15,TextFace::Classic))+28;x-=w;
  round_rect(x,chip_y,w,42,9,0x0A0C18,184,g_theme.white,52);face_text(x+14,chip_y+5,keys[i],0x8A90B4,11,TextFace::Classic);face_text(x+14,chip_y+19,value,0xE4E6F0,15,TextFace::Classic);x-=10;}
 // Progress bar and the times under it.
 const int px=57,py=628,pw=1223-57,ph=6;round_rect(px,py,pw,ph,3,g_theme.white,41);
 const uint64_t shown=osd.scrubbing?osd.target:osd.position;int fill=0;
 if(!osd.live&&osd.duration)fill=(int)(pw*std::min(1.0,(double)shown/osd.duration));
 if(fill>0)round_rect(px,py,fill,ph,3,g_theme.accent,255);
 if(osd.scrubbing&&!osd.live){circle(px+fill,py+ph/2,7,g_theme.white,255);std::string at=clock_text(osd.target);int w=face_text_width(at,16,TextFace::Classic)+20,lx=std::max(px,std::min(px+pw-w,px+fill-w/2));round_rect(lx,py-38,w,26,6,0x0A0C18,210,g_theme.accent_alt,200);face_text(lx+10,py-34,at,g_theme.white,16,TextFace::Classic);}
 if(osd.live){int w=face_text_width("LIVE",12,TextFace::Classic)+16;round_rect(px,644,w,18,9,0xD8344A,235);face_text(px+8,646,"LIVE",g_theme.white,12,TextFace::Classic);}
 else{face_text(px,644,clock_text(shown),0xE0E0E0,15,TextFace::Classic);if(osd.duration){std::string rem="-"+clock_text(osd.duration>shown?osd.duration-shown:0);face_text(px+pw-face_text_width(rem,15,TextFace::Classic),644,rem,0xE0E0E0,15,TextFace::Classic);}}
 // Hints: what X does, seeking, stop and hide.
 if(osd.live)draw_hints(57,680,{{'X',osd.paused?"Play":"Pause"},{'C',"Stop"},{'S',"Hide"}});
 else draw_hints(57,680,{{'X',osd.paused?"Play":"Pause"},{'D',"Seek 10s"},{'L',""},{'R',"Skip 5 min"},{'C',"Stop"},{'S',"Hide"}});
}

// ---- about (PS3 Settings > About) -------------------------------------------------
namespace {
enum AboutKind { A_HEAD, A_VER, A_ITEM, A_NAME, A_TEXT, A_GAP };
struct AboutLine { AboutKind kind; const char* a; const char* b; };
// Updates: the PS4 releases, newest first. Add each new release at the top.
const AboutLine about_updates[]={
 {A_VER,"1.09",nullptr},
 {A_ITEM,"A startup animation plays while Home loads.",nullptr},
 {A_ITEM,"Home rows, the A-Z jump bar and a full details page with the cast.",nullptr},
 {A_ITEM,"Live TV gets a channel grid, categories, a guide and favourites.",nullptr},
 {A_ITEM,"A player bar with progress, times and scrubbing.",nullptr},
 {A_ITEM,"The Artwork screensaver, with its own on / off and delay settings.",nullptr},
 {A_ITEM,"Faster library pages; the next page is fetched ahead.",nullptr},
 {A_ITEM,"Smoother scrolling and transitions.",nullptr},
 {A_ITEM,"Every button hint uses controller icons, and Settings has an About page.",nullptr},
 {A_VER,"1.08",nullptr},
 {A_ITEM,"Movie and show grids get the Classic layout and fonts.",nullptr},
 {A_ITEM,"Playback uses software H264 decoding at 720p.",nullptr},
 {A_VER,"1.07",nullptr},
 {A_ITEM,"Playback moves to MPV and FFmpeg.",nullptr},
 {A_VER,"1.06",nullptr},
 {A_ITEM,"L1 / R1 move between Home, Search and Settings.",nullptr},
 {A_VER,"1.05",nullptr},
 {A_ITEM,"Triangle opens the quick peek.",nullptr},
 {A_VER,"1.04",nullptr},
 {A_ITEM,"Live TV channels and playback progress reporting.",nullptr},
 {A_VER,"1.03",nullptr},
 {A_ITEM,"The app is titled Jellyfin [PS].",nullptr},
 {A_VER,"1.02",nullptr},
 {A_ITEM,"Quick Connect sign-in and the QWERTY keyboard.",nullptr},
};
const AboutLine about_credits[]={
 {A_HEAD,"Made by",nullptr},
 {A_NAME,"jw-notes","Jellyfin [PS] for PS4"},
};
const AboutLine about_licences[]={
 {A_HEAD,"License",nullptr},
 {A_TEXT,"JellyFin [PS] is free software under the GNU GPL, version 3 or later.",nullptr},
 {A_TEXT,"It comes with no warranty. Each component keeps its own licence.",nullptr},
 {A_TEXT,"Source code and the full licence texts:",nullptr},
 {A_TEXT,"github.com/jw-notes",nullptr},
 {A_GAP,nullptr,nullptr},
 {A_HEAD,"Open-source software",nullptr},
 {A_NAME,"MPV","GPL 2+"},
 {A_NAME,"FFmpeg","LGPL / GPL 3"},
 {A_NAME,"Switchfin PS4 integration","Apache 2.0"},
 {A_NAME,"SDL2, zlib, bzip2","zlib / BSD-style"},
 {A_NAME,"mbedTLS","Apache 2.0"},
 {A_NAME,"dav1d","BSD 2-Clause"},
 {A_NAME,"libass, FreeType, FriBidi, libpng","ISC / FTL / LGPL / libpng"},
 {A_NAME,"stb_image, stb_truetype","Public domain / MIT"},
 {A_NAME,"OpenOrbis PS4 Toolchain","GPL 3"},
 {A_GAP,nullptr,nullptr},
 {A_HEAD,"Fonts and art",nullptr},
 {A_NAME,"Noto Sans JP","SIL OFL 1.1"},
 {A_NAME,"Open Sans, Roboto Condensed","Apache 2.0"},
 {A_NAME,"Satoshi","ITF Free Font License"},
 {A_NAME,"PlayStation button sprites (Kenney)","CC0"},
 {A_NAME,"Jellyfin logo","CC BY-SA 4.0"},
};
struct AboutSection{const char* label;const AboutLine* lines;int n;};
const AboutSection about_sections[]={{"Updates",about_updates,(int)(sizeof about_updates/sizeof *about_updates)},{"Credits",about_credits,(int)(sizeof about_credits/sizeof *about_credits)},{"Licences",about_licences,(int)(sizeof about_licences/sizeof *about_licences)}};
int about_line_h(AboutKind k){return k==A_HEAD?40:k==A_VER?34:k==A_GAP?14:27;}
constexpr int ABOUT_TOP=190,ABOUT_BOTTOM=640;
}
int about_sections_count(){return 3;}
int about_max_scroll(int section){int total=0;const auto& s=about_sections[section];for(int i=0;i<s.n;++i)total+=about_line_h(s.lines[i].kind);return std::max(0,total-(ABOUT_BOTTOM-ABOUT_TOP));}
void draw_about(int section,int top){
 background(0,HEIGHT);const int col_w=860,x0=(WIDTH-col_w)/2;const auto& s=about_sections[section];int y=ABOUT_TOP-top;
 for(int i=0;i<s.n;++i){const auto& l=s.lines[i];const int h=about_line_h(l.kind);
  if(y+h>ABOUT_TOP-10&&y<ABOUT_BOTTOM){switch(l.kind){
   case A_HEAD:face_text(x0,y+12,l.a,g_theme.accent_alt,18,TextFace::Classic);break;
   case A_VER:face_text(x0,y+8,"Version "+std::string(l.a),g_theme.accent_alt,18,TextFace::Classic);break;
   case A_ITEM:face_text(x0+6,y,"\xC2\xB7",g_theme.accent,16,TextFace::Classic);face_text(x0+22,y,fit_face(l.a,col_w-22,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic);break;
   case A_NAME:{face_text(x0,y,l.a,g_theme.text,17,TextFace::Classic);if(l.b){int bw=face_text_width(l.b,15,TextFace::Classic);face_text(x0+col_w-bw,y+1,l.b,g_theme.text_dim,15,TextFace::Classic);}break;}
   case A_TEXT:face_text(x0,y,fit_face(l.a,col_w,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic);break;
   default:break;}}
  y+=h;}
 // The list scrolls under the title, the tabs and the hints: repaint both ends.
 background(0,ABOUT_TOP-15);rect(x0,ABOUT_TOP-16,col_w,1,g_theme.hairline);
 header_chrome("Settings",-1);
 const std::string title="Jellyfin [PS]";face_text((WIDTH-face_text_width(title,32,TextFace::Classic))/2,62,title,g_theme.text,32,TextFace::Classic);
#ifdef JF_BUILD_VERSION
 std::string version="Version " JF_BUILD_VERSION;
#else
 std::string version="Version host";
#endif
 if(version.compare(8,1,"0")==0)version.erase(8,1);
 face_text((WIDTH-face_text_width(version,16,TextFace::Classic))/2,106,version,g_theme.accent,16,TextFace::Classic);
 int widths[3],total=0;for(int i=0;i<3;++i){widths[i]=face_text_width(about_sections[i].label,18,TextFace::Classic);total+=widths[i]+(i?44:0);}
 int x=(WIDTH-total)/2;for(int i=0;i<3;++i){bool on=i==section;face_text(x,140,about_sections[i].label,on?g_theme.white:g_theme.text_faint,18,TextFace::Classic);if(on)rect(x,166,widths[i],3,g_theme.accent);x+=widths[i]+44;}
 background(ABOUT_BOTTOM,650);
 hint_bar({{'l',""},{'r',"Section"},{'D',"Scroll"},{'C',"Back"}});
}
