#include "ui_view.h"
#include "mono_clock.h"
#include "platform.h"
#include "art.h"
#include "boot_anim.h"
#include "language.h"
#include <algorithm>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../vendor/ps3/source/ui/render/peek.h"
#include "../vendor/ps3/source/ui/render/clock_fmt.h"
#ifndef HOST_TEST
#include <orbis/SystemService.h>
#endif
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
std::string fit_face(const std::string& value,int width,int size,TextFace face){if(face_text_width(value,size,face)<=width)return value;std::string s=value;while(!s.empty()&&face_text_width(s+"...",size,face)>width){size_t end=s.size()-1;while(end&&((unsigned char)s[end]&0xc0)==0x80)--end;s.resize(end);}return s+"...";}
std::string clock_text(uint64_t ms){unsigned s=(unsigned)(ms/1000);char b[24];if(s>=3600)snprintf(b,sizeof b,"%u:%02u:%02u",s/3600,s/60%60,s%60);else snprintf(b,sizeof b,"%u:%02u",s/60,s%60);return b;}
static std::string runtime_text(uint64_t ms){unsigned m=(unsigned)((ms+59999)/60000);char b[24];if(m>=60)snprintf(b,sizeof b,"%uh %um",m/60,m%60);else snprintf(b,sizeof b,"%um",m);return b;}
// The console's clock (PS3 clock_fmt.h): time() is UTC and the C library
// knows no zone, so the offset, summer time, 12/24 h and date order come from
// the system settings, read again every half minute.
void console_clock(uint64_t after_ms,std::string& time_text,std::string& date_text){
 static int offset=0,date_fmt=CLK_DATE_MDY,time_fmt=CLK_TIME_12H;static time_t read_at=0;const time_t now=time(nullptr);
 if(!read_at||now-read_at>=30||now<read_at){read_at=now;
#ifdef HOST_TEST
  tm local{};localtime_r(&now,&local);offset=(int)local.tm_gmtoff;
#else
  int32_t v=0;offset=0;
  if(sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE,&v)>=0)offset=v*60;
  if(sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME,&v)>=0&&v)offset+=3600;
  if(sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT,&v)>=0&&v>=0&&v<=2)date_fmt=v;
  if(sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT,&v)>=0&&(v==0||v==1))time_fmt=v;
#endif
 }
 char t[16],d[16];clock_format((long long)now+(long long)(after_ms/1000),offset,date_fmt,time_fmt,t,sizeof t,d,sizeof d);time_text=t;date_text=d;
}
std::string ends_at(uint64_t remaining_ms){std::string t,d;console_clock(remaining_ms,t,d);return TF("Ends at %s",t.c_str());}
static void avatar(int x,int y,int d);
// The PS3 loading shimmer (ui_depth.cpp): a soft light band sweeping across a
// placeholder every 1.4 s, later for cards further right so a row reads as
// one wave.
static void shimmer(int x,int y,int w,int h){
 if(w<=0||h<=0)return;const uint64_t t=mono_us()+(uint64_t)std::max(0,x)*400000/WIDTH;
 const float u=(t%1400000)/1400000.0f,hb=w*0.45f,c=x-hb+u*(w+2*hb);
 for(int px=std::max(x,(int)(c-hb));px<std::min(x+w,(int)(c+hb)+1);++px){const float a=0.09f*(1-fabsf((px+0.5f-c)/hb));if(a>0)blend_rect(px,y,1,h,0xFFFFFF,(int)(a*255+0.5f));}
}
// Artwork with its placeholder's two states: still loading (the shimmer) or
// no image at all (false: the caller draws its fallback). A fresh image fades in.
static bool card_art(const std::string& key,int x,int y,int w,int h,bool cover){
 // Navigation shortcuts (nav:...) have no server image to wait for.
 if(key.rfind("nav:",0)==0)return false;
 const int state=art_state(key);
 if(state>0){const int a=boot_active()?255:(int)(255*art_fade(key)+0.5f);return cover?draw_art_cover(key,x,y,w,h,true,a):draw_art(key,x,y,w,h,true,a);}
 if(state==0)shimmer(x,y,w,h);
 return state==0;
}
// The background gradient, one colour per row: composed once per theme and
// copied, instead of a row of fills on every frame of every screen.
static void background(int y0,int y1){
 static std::vector<uint32_t> rows;static uint32_t top=1,bottom=1;
 if(rows.empty()||top!=g_theme.bg_top||bottom!=g_theme.bg_bot){top=g_theme.bg_top;bottom=g_theme.bg_bot;rows.resize(HEIGHT);for(int y=0;y<HEIGHT;++y)rows[y]=0xff000000|mix(top,bottom,y*255/(HEIGHT-1));}
 auto fb=framebuffer();for(int y=std::max(0,y0);y<std::min(HEIGHT,y1);++y)std::fill(fb+y*WIDTH,fb+(y+1)*WIDTH,rows[y]);
}
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
 // Right of the tabs, as on the PS3: the date, the clock and who is signed in.
 {const int d=34,ax=WIDTH-48-d,ay=14;int right=WIDTH-48;
  if(g_userid[0]&&!boot_header_pose()){
   // A user with no picture (or one that fails) gets the default silhouette.
   static std::string missing;const std::string key=std::string("user:")+g_userid;const int state=art_state(key);
   if(state<0)missing=g_userid;else if(state==0&&missing!=g_userid)load_image(key,"/Users/"+encode(g_userid)+"/Images/Primary?MaxWidth=96&Quality=90");
   if(state<=0||!draw_art_circle(key,ax,ay,d))avatar(ax,ay,d);ring(ax+d/2,ay+d/2,d/2,g_theme.hairline);right=ax-16;}
  std::string t,dt;console_clock(0,t,dt);const int a=boot_header_pose()?(int)(255*std::max(0.0f,std::min(1.0f,boot_header_pose()->word_alpha[7]))):255;
  if(a>0){const int tw=face_text_width(t,17,TextFace::Classic),dw=face_text_width(dt,13,TextFace::Classic);face_text(right-tw,20,t,mix(g_theme.bg_top,g_theme.text,a),17,TextFace::Classic);face_text(right-tw-10-dw,24,dt,mix(g_theme.bg_top,g_theme.text_faint,a),13,TextFace::Classic);}}
 int widths[3],total=20;for(int i=0;i<3;++i){widths[i]=face_text_width(T(header_tabs[i]),17,TextFace::Classic)+36;total+=widths[i];}int x=(WIDTH-total)/2;
 for(int i=0;i<3;++i){bool focused=i==focused_tab,active=tab==header_tabs[i];if(!tab.empty()){if(focused)rect(x,14,widths[i],34,g_theme.accent);else if(active)rect(x+14,45,widths[i]-28,3,g_theme.accent);face_text(x+18,20,T(header_tabs[i]),focused?g_theme.bg:active?g_theme.text:g_theme.text_dim,17,TextFace::Classic);}x+=widths[i]+10;}
}
void app_header(const std::string& title,const std::string& tab,int focused_tab){
 background(0,HEIGHT);header_chrome(tab,focused_tab);
 if(!title.empty())face_text(64,86,fit_face(title,1100,24,TextFace::Classic),g_theme.text,24,TextFace::Classic);
}
void hint_bar(std::initializer_list<Hint> hints){rect(0,650,WIDTH,70,g_theme.bg_bot);rect(0,650,WIDTH,1,mix(g_theme.bg_bot,g_theme.text_faint,60));draw_hints(64,674,hints);}
static std::string kind(const Item& item){const char* type=item.base.type;if(!strcmp(type,"CollectionFolder")||!strcmp(type,"UserView"))return T("Library");if(!strcmp(type,"MusicAlbum"))return T("Album");if(!strcmp(type,"MusicArtist"))return T("Artist");if(!strcmp(type,"TvChannel"))return T("Live TV");if(!strcmp(type,"Navigation"))return T("Browse");return T(type);}
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
 const std::string search_prefix=TF("Search: %s","");app_header(home?T("My Media"):"",title.rfind(search_prefix,0)==0?"Search":"Home",focused_tab);
 if(!home){face_text(g.x0,86,fit_face(title,900,24,TextFace::Classic),g_theme.text,24,TextFace::Classic);
  std::string sub=breadcrumb;if(sub.empty()&&jump&&!jump->filter.empty())sub=jump->filter=="#"?T("Names before A"):TF("Starting with %s",jump->filter.c_str());
  if(!sub.empty())face_text(g.x0,122,fit_face(sub,g.grid_w,15,TextFace::Classic),g_theme.text_dim,15,TextFace::Classic);}
 int last=std::min(first+visible,(int)page.items.size());std::string count=page.items.empty()?T("No items"):std::to_string(start+first+1)+" - "+std::to_string(start+last)+" / "+std::to_string(page.total);
 if(home)text(48,143,count,g_theme.text_dim);else face_text(WIDTH-g.x0-face_text_width(count,14,TextFace::Classic),95,count,g_theme.text_dim,14,TextFace::Classic);
 for(int i=first;i<last;++i){const auto& item=page.items[i];int relative=i-first,x=g.x0+(relative%g.cols)*(g.card_w+gap),y=top+(relative/g.cols)*g.stride;bool focus=i==selected&&!(jump&&jump->active);
  rect(x,y,g.card_w,g.card_h,g_theme.panel);bool image=card_art(item.base.id,x,y,g.card_w,g.card_h,home);
  if(!image)face_text(x+10,y+g.card_h/2-12,fit_face(item.base.name,g.card_w-20,18,TextFace::Classic),g_theme.text_dim,18,TextFace::Classic,g.card_w-20);
  if(focus)outline(x-3,y-3,g.card_w+6,g.card_h+6,g_theme.focus_ring,2);
  if(home){text(x,y+g.card_h+12,fit_text(item.base.name,g.card_w),focus?g_theme.text:g_theme.text_dim,24,g.card_w);text(x,y+g.card_h+42,kind(item),g_theme.text_faint,24,g.card_w);}
  else{bool wide=g.card_w>g.card_h;int size=wide?(focus?20:16):(focus?21:20);TextFace face=wide?TextFace::Classic:TextFace::Poster;face_text(x,y+g.card_h+8,fit_face(item.base.name,g.card_w,size,face),focus?g_theme.text:g_theme.text_dim,size,face,g.card_w);
   if(focus){std::string meta=item.year?std::to_string(item.year):"";if(item.duration){if(!meta.empty())meta+="  ";meta+=std::to_string(item.duration/60000)+" min";}face_text(x,y+g.card_h+34,fit_face(meta,g.card_w,14,TextFace::Poster),g_theme.text_dim,14,TextFace::Poster,g.card_w);}}
  if(item.duration&&item.resume){int fill=(int)(g.card_w*std::min(1.0,(double)item.resume/item.duration));rect(x,y+g.card_h-3,g.card_w,3,g_theme.track);rect(x,y+g.card_h-3,fill,3,g_theme.accent);}
 }
 if(!home&&page.items.empty()&&jump&&!jump->filter.empty())face_text(g.x0,top+36,TF("Nothing starts with %s. Press Left to pick another letter.",jump->filter.c_str()),g_theme.text_dim,18,TextFace::Classic);
 if(!home&&jump&&jump->show){int current=jump->active?jump->sel:!jump->filter.empty()?jump_entry(jump->filter=="#"?"":jump->filter):page.items.empty()?1:jump_entry(page.items[std::min(selected,(int)page.items.size()-1)].base.name);draw_jump_bar(*jump,std::max(8,g.x0-46),top,640,current);}
 if(home&&!page.items.empty()){const auto& item=page.items[selected];text(48,430,fit_text(item.base.name,1130,32),g_theme.text,32);paragraph(48,478,item.overview.empty()?(is_container(item.base.type)?T("Open this library to browse your media."):T("Open for details, versions and playback options.")):item.overview,1150,4,g_theme.text_dim);}
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
void draw_home_header(int header_focus){background(0,HOME_CLIP);header_chrome("Home",header_focus);}
// Before Home first shows: fetch the artwork of the cards on screen and size
// it for its card, so the first frames are copies rather than scaling.
void home_prepare_art(const HomeView& home,int timeout_ms){
 struct Card{std::string key,fallback;int w,h;};std::vector<Card> cards;
 for(size_t i=0;i<home.rows.size();++i){const auto& row=home.rows[i];const int y=home_row_y(home,(int)i);if(y>HOME_BOTTOM||y+home_row_height(row)<HOME_CLIP)continue;
  int w,h;home_card_size(row.shape,w,h);const int step=home_step_px(row);
  for(size_t c=0;c<row.page.items.size();++c){const int x=HOME_X0+(int)c*step-(int)lroundf(row.offset);if(x>WIDTH||x+w<0)continue;
   std::string key,path;home_image(row,row.page.items[c],key,path);if(key.rfind("nav:",0)==0)continue;load_image(key,path);
   cards.push_back({key,key!=row.page.items[c].base.id?std::string(row.page.items[c].base.id):"",w,h});}}
 const uint64_t until=mono_ms()+timeout_ms;
 for(;;){bool waiting=false;for(const auto& c:cards)if(art_state(c.key)==0)waiting=true;if(!waiting||mono_ms()>=until)break;usleep(20000);}
 for(const auto& c:cards){if(art_state(c.key)>0)art_prepare(c.key,c.w,c.h);else if(!c.fallback.empty()){load_art(c.fallback);}}
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
   // A wide card falls back to the poster if the backdrop is missing.
   bool image=key!=item.base.id&&art_state(key)<0?card_art(item.base.id,cx,cy,w,h,true):card_art(key,cx,cy,w,h,true);
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
 if(home.rows.empty())face_text(HOME_X0,HOME_TOP+36,home.loaded?T("Nothing to show yet. Add a library on your Jellyfin server."):T("Loading..."),g_theme.text_dim,18,TextFace::Classic);
 if(header_focus>=0)hint_bar({{'D',"Nav"},{'X',"Select"},{'l',""},{'r',"Tabs"}});
 else if(home.focus>=0&&home.focus<(int)home.rows.size()&&home.rows[home.focus].shape==SHAPE_LIBRARY)hint_bar({{'D',"Nav"},{'X',"Open"},{'S',"Favourites"},{'l',""},{'r',"Tabs"}});
 else hint_bar({{'D',"Nav"},{'X',"Open"},{'T',"Details"},{'S',"Favourites"},{'l',""},{'r',"Tabs"}});
}

// ---- search ----------------------------------------------------------------
void draw_search(const std::string& query){
 app_header(T("Search your library"),"Search");
 rect(48,190,1184,88,g_theme.panel_hi);outline(48,190,1184,88,g_theme.focus_ring,2);
 text(72,219,fit_text(query.empty()?T("Enter a title, show, artist or album"):query,1120,32),query.empty()?g_theme.text_dim:g_theme.text,32,1120);
 text(48,330,T("Search across your Jellyfin libraries."),g_theme.text_dim);
 hint_bar({{'X',"Enter search"},{'C',"Back"},{'l',"Home"},{'r',"Settings"}});
}

// ---- quick peek ----------------------------------------------------------------
// A rectangle at a fractional position: whole pixels inside, the edge pixels
// covered by the fraction they overlap. Animated panels glide instead of
// stepping a pixel at a time.
static void soft_rect(float x,float y,float w,float h,uint32_t color,int alpha=255){
 if(w<=0||h<=0||alpha<=0)return;const int x0=(int)floorf(x),x1=(int)ceilf(x+w),ix0=(int)ceilf(x),ix1=(int)floorf(x+w);
 for(int py=(int)floorf(y);py<(int)ceilf(y+h);++py){const float cy=std::min((float)py+1,y+h)-std::max((float)py,y);if(cy<=0)continue;const int a=(int)(alpha*cy+0.5f);
  if(ix1>ix0)blend_rect(ix0,py,ix1-ix0,1,color,a);
  for(int px=x0;px<x1;++px){if(ix1>ix0&&px>=ix0&&px<ix1)continue;const float cx=std::min((float)px+1,x+w)-std::max((float)px,x);if(cx>0)blend_rect(px,py,1,1,color,(int)(a*cx+0.5f));}}
}
static void soft_frame(float x,float y,float w,float h,float t,uint32_t color){soft_rect(x,y,w,t,color);soft_rect(x,y+h-t,w,t,color);soft_rect(x,y+t,t,h-2*t,color);soft_rect(x+w-t,y+t,t,h-2*t,color);}
void draw_quick_peek(const Item& item,const PeekOrigin& origin,float progress,const std::string& status,const uint32_t* under){
 const peek_rect from={(float)origin.x,(float)origin.y,(float)origin.w,(float)origin.h};
 const auto frame=peek_eval(progress,from,peek_panel(WIDTH/2.0f,HEIGHT/2.0f,520));
 // The page dims behind the card; from `under` that is one pass, not a copy and a blend.
 const int dim=(int)(frame.lift*170);
 if(under){const uint32_t keep=256-(uint32_t)(dim+1);auto fb=framebuffer();for(int i=0;i<WIDTH*HEIGHT;++i){const uint32_t d=under[i];fb[i]=0xff000000|((((d&0xff00ff)*keep)>>8)&0xff00ff)|((((d&0x00ff00)*keep)>>8)&0x00ff00);}}
 else blend_rect(0,0,WIDTH,HEIGHT,0,dim);
 const float fx=frame.r.x,fy=frame.r.y,fw=frame.r.w,fh=frame.r.h;
 soft_rect(fx,fy,fw,fh,g_theme.panel_hi);
 if(!frame.back){draw_art_smooth(item.base.id,fx,fy,fw,fh);soft_frame(fx,fy,fw,fh,2,g_theme.focus_ring);return;}
 soft_frame(fx,fy,fw,fh,2,g_theme.focus_ring);
 const int x=(int)lroundf(fx),y=(int)lroundf(fy),w=(int)lroundf(fw),h=(int)lroundf(fh);
 if(frame.content_a<0.01f)return;
 const auto ink=mix(g_theme.panel_hi,g_theme.text,(int)(255*frame.content_a));
 const auto muted=mix(g_theme.panel_hi,g_theme.text_dim,(int)(255*frame.content_a));
 const int tx=x+210,tw=w-234;
 draw_art(item.base.id,x+24,y+24,160,240);
 text(tx,y+26,fit_text(item.base.name,tw,32),ink,32,tw);
 std::string metadata=kind(item);if(!episode_code(item).empty())metadata+="  "+episode_code(item);if(item.year)metadata+="  "+std::to_string(item.year);if(item.duration)metadata+="  "+runtime_text(item.duration);
 if(item.duration&&!is_container(item.base.type))metadata+="  "+ends_at(item.resume&&settings.resume&&item.resume<item.duration?item.duration-item.resume:item.duration);
 text(tx,y+73,fit_text(metadata,tw),muted,24,tw);
 text(tx,y+108,fit_text(item.rating+(item.rating.empty()?"":"  ")+item.genres,tw),muted,24,tw);
 const bool cast=!item.people.empty();
 paragraph(tx,y+151,item.overview.empty()?(status.empty()?T("No synopsis available."):T(status.c_str())):item.overview,tw,cast?6:9,ink);
 // The cast, as on the detail page: round photos, names and roles under
 // them, across the card's foot. The photos load as the card opens.
 if(cast){
  constexpr int KD=64,KP=92;const int KY=y+330,count=std::min((int)item.people.size(),(w-48)/KP);
  const auto faint=mix(g_theme.panel_hi,g_theme.text_faint,(int)(255*frame.content_a));
  for(int k=0;k<count;++k){const auto& person=item.people[k];const int kx=x+24+k*KP,px=kx+(KP-KD)/2;
   if(person.image&&!person.id.empty())load_image("person:"+person.id,"/Items/"+encode(person.id)+"/Images/Primary?MaxWidth=200&Quality=85");
   if(!(person.image&&draw_art_circle("person:"+person.id,px,KY,KD)))avatar(px,KY,KD);ring(px+KD/2,KY+KD/2,KD/2,g_theme.hairline);
   const std::string name=fit_face(person.name,KP-6,13,TextFace::Classic),role=fit_face(person.role,KP-6,12,TextFace::Classic);
   face_text(kx+(KP-face_text_width(name,13,TextFace::Classic))/2,KY+KD+6,name,ink,13,TextFace::Classic);
   if(!role.empty())face_text(kx+(KP-face_text_width(role,12,TextFace::Classic))/2,KY+KD+24,role,faint,12,TextFace::Classic);}
 }
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
 if(id==ACT_RESUME)return TF("Resume %s",clock_text(item.resume).c_str());if(id==ACT_PLAY)return T("Play");if(id==ACT_START)return T("Play from start");
 if(id==ACT_FAVOURITE)return T(item.favourite?"Favourited":"Favourite");return T(item.played?"Watched":"Mark as watched");
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
 rect(PX,PY,PW,PH,g_theme.thumb_dim);if(!card_art(item.base.id,PX,PY,PW,PH,true))face_text(PX+16,PY+PH/2-12,fit_face(item.base.name,PW-32,20,TextFace::Classic),g_theme.text_dim,20,TextFace::Classic);outline(PX-1,PY-1,PW+2,PH+2,g_theme.hairline,1);
 // Title (series above it for an episode).
 int y=72;if(!strcmp(item.base.type,"Episode")&&!item.series.empty()){face_text(TX,y,fit_face(item.series,TW,17,TextFace::Classic),g_theme.text_dim,17,TextFace::Classic);y+=22;}
 face_text(TX,y,fit_face(item.base.name,TW,34,TextFace::Classic),g_theme.text,34,TextFace::Classic);
 // Metadata: year · runtime · certificate · SxEy, then the score row.
 std::vector<std::string> parts;if(!episode_code(item).empty())parts.push_back(episode_code(item));if(item.year)parts.push_back(std::to_string(item.year));if(item.duration)parts.push_back(runtime_text(item.duration));if(!item.rating.empty())parts.push_back(item.rating);
 int mx=TX;const int my=y+50;for(size_t i=0;i<parts.size();++i){if(i){face_text(mx+6,my,"\xC2\xB7",g_theme.text_faint,17,TextFace::Classic);mx+=20;}face_text(mx,my,parts[i],g_theme.text_dim,17,TextFace::Classic);mx+=face_text_width(parts[i],17,TextFace::Classic);}
 int sx=TX;const int sy=my+27;
 if(!item.community.empty()){draw_star(sx,sy+1,17,g_theme.accent_alt);sx+=24;face_text(sx,sy,item.community+"/10",g_theme.accent_alt,17,TextFace::Classic);sx+=face_text_width(item.community+"/10",17,TextFace::Classic)+24;}
 if(item.played){face_text(sx,sy,T("Watched"),g_theme.text_dim,17,TextFace::Classic);sx+=face_text_width(T("Watched"),17,TextFace::Classic)+24;}
 const bool resuming=item.duration&&item.resume&&settings.resume&&item.resume<item.duration;
 if(resuming){std::string left=TF("%s left",runtime_text(item.duration-item.resume).c_str());face_text(sx,sy,left,g_theme.text_dim,17,TextFace::Classic);sx+=face_text_width(left,17,TextFace::Classic)+24;}
 // When it would finish if played now: from the start while that button has the focus (PS3).
 if(item.duration){const auto acts=detail_actions(item);const bool from_start=view.row==0&&view.col<(int)acts.size()&&acts[view.col]==ACT_START;
  face_text(sx,sy,ends_at(resuming&&!from_start?item.duration-item.resume:item.duration),g_theme.text_dim,17,TextFace::Classic);}
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
 const std::string names[]={T("Version"),T("Audio"),T("Subtitles")},values[]={view.version,view.audio.empty()?"Server default":view.audio,view.subtitles};
 int selx=TX;const int SY=AY+AH+14,SH=36;
 for(int i=0;i<3;++i){int nw=face_text_width(names[i],14,TextFace::Classic);std::string value=fit_face(values[i],330-nw-40,16,TextFace::Classic);int w=std::min(330,nw+12+face_text_width(value,16,TextFace::Classic)+30);if(selx+w>TX+TW)w=TX+TW-selx;if(w<80)break;
  round_rect(selx,SY,w,SH,5,g_theme.panel,230,g_theme.hairline,255);face_text(selx+15,SY+10,names[i],g_theme.text_faint,14,TextFace::Classic);face_text(selx+15+nw+12,SY+9,fit_face(value,w-nw-42,16,TextFace::Classic),view.row==1&&view.col==i?g_theme.white:g_theme.text,16,TextFace::Classic);
  if(view.row==1&&view.col==i)focus_round(selx,SY,w,SH,5);selx+=w+12;}
 // Overview, then labelled rows as on Jellyfin's web page.
 int oy=SY+SH+18;std::string overview=item.overview;if(overview.empty()&&!item.tagline.empty())overview=item.tagline;
 if(!overview.empty()){paragraph(TX,oy,overview,TW,item.people.empty()?6:3,g_theme.text_dim,17,23);oy+=(item.people.empty()?6:3)*23+8;}
 const int vx=TX+100;
 if(!item.genres.empty()){face_text(TX,oy,T("Genres"),g_theme.text_faint,15,TextFace::Classic);face_text(vx,oy,fit_face(item.genres,TX+TW-vx,15,TextFace::Classic),g_theme.text,15,TextFace::Classic);oy+=22;}
 if(!item.director.empty()){face_text(TX,oy,T("Director"),g_theme.text_faint,15,TextFace::Classic);face_text(vx,oy,fit_face(item.director,TX+TW-vx,15,TextFace::Classic),g_theme.text,15,TextFace::Classic);oy+=22;}
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
 // Play / pause disc and the title beside it. Live TV can't be paused: no
 // disc, the title starts at the margin.
 const int d=50,bx=57,by=529;
 if(!osd.live){
  // Focused (the PS3 HUD's default focus): a halo and the accent_alt border.
  if(osd.control==OSD_PLAY)round_rect(bx-5,by-5,d+10,d+10,(d+10)/2,g_theme.accent,60);
  round_rect(bx,by,d,d,d/2,g_theme.accent,190,osd.control==OSD_PLAY?g_theme.accent_alt:g_theme.white,osd.control==OSD_PLAY?255:70);
  const int cx=bx+d/2,cy=by+d/2;
  if(osd.paused||!osd.started){const int th=d*26/64,tw=th*7/8,tx=cx-tw/2+d/32,ty=cy-th/2;for(int r=0;r<th;++r){float f=(float)r/(th-1);int w=(int)((1.0f-fabsf(2.0f*f-1.0f))*tw+0.5f);if(w>0)rect(tx,ty+r,w,1,g_theme.white);}}
  else{const int bh=d*24/64,bw=d*7/64,gap=d*7/64;rect(cx-gap/2-bw,cy-bh/2,bw,bh,g_theme.white);rect(cx+gap/2,cy-bh/2,bw,bh,g_theme.white);}
 }
 // The title stops short of the chips at the right.
 const int tx0=osd.live?bx:bx+d+20;face_text(tx0,by+(osd.subtitle.empty()?9:0),fit_face(osd.title,580,30,TextFace::Classic),g_theme.white,30,TextFace::Classic);
 if(!osd.subtitle.empty())face_text(tx0,by+36,fit_face(osd.subtitle,580,15,TextFace::Classic),0xC2C2C2,15,TextFace::Classic);
 // The PS3 HUD's chips, right-aligned to x=1223, left to right: AUDIO,
 // VOLUME, SUBTITLES, PICTURE. Volume and Picture are focus stops (Left /
 // Right reach them, X uses them); focus is the accent_alt border with a soft
 // accent halo. The volume slider opens above its chip while adjusting.
 int x=1223;const int chip_y=by+6;
 {const std::string vol=std::to_string(osd.volume)+"%";
  const std::string ckeys[4]={T("AUDIO"),T("VOLUME"),T("SUBTITLES"),T("PICTURE")};
  const std::string cvalues[4]={fit_face(osd.audio.empty()?T("Default"):osd.audio,240,15,TextFace::Classic),vol,fit_face(osd.subtitles.empty()?T("Off"):osd.subtitles,240,15,TextFace::Classic),T(osd.picture.empty()?"Fit":osd.picture.c_str())};
  const int slot[4]={-1,OSD_VOLUME,-1,OSD_PICTURE};
  // PICTURE carries an icon left of its value: the screen, and the picture in
  // it as Fit (bars above and below), Fill (all of it) or Stretch (arrows).
  constexpr int ICON_W=20,ICON_H=13,ICON_GAP=8;
  for(int i=3;i>=0&&!osd.audio_only;--i){const int icon=slot[i]==OSD_PICTURE?ICON_W+ICON_GAP:0;
   int w=std::max(face_text_width(ckeys[i],11,TextFace::Classic),face_text_width(cvalues[i],15,TextFace::Classic)+icon)+28;if(slot[i]==OSD_VOLUME)w=std::max(w,76);x-=w;
   const bool foc=slot[i]>=0&&osd.control==slot[i];
   if(foc)round_rect(x-4,chip_y-4,w+8,50,13,g_theme.accent,46);
   round_rect(x,chip_y,w,42,9,0x0A0C18,184,foc?g_theme.accent_alt:g_theme.white,foc?255:52);
   face_text(x+14,chip_y+5,ckeys[i],foc?0xC9C3F2:0x8A90B4,11,TextFace::Classic);face_text(x+14+icon,chip_y+19,cvalues[i],foc?g_theme.white:0xE4E6F0,15,TextFace::Classic);
   if(icon){const int ix=x+14,iy=chip_y+22;const uint32_t c=foc?g_theme.white:0xB8BCD0;
    rect(ix,iy,ICON_W,1,c);rect(ix,iy+ICON_H-1,ICON_W,1,c);rect(ix,iy,1,ICON_H,c);rect(ix+ICON_W-1,iy,1,ICON_H,c);
    if(osd.picture=="Fill")rect(ix+2,iy+2,ICON_W-4,ICON_H-4,foc?g_theme.accent_alt:c);
    else if(osd.picture=="Stretch"){rect(ix+2,iy+2,ICON_W-4,ICON_H-4,foc?g_theme.accent_alt:c);for(int k=0;k<3;++k){rect(ix+3+k,iy+ICON_H/2-k,1,2*k+1,0x0A0C18);rect(ix+ICON_W-4-k,iy+ICON_H/2-k,1,2*k+1,0x0A0C18);}}
    else rect(ix+2,iy+4,ICON_W-4,ICON_H-8,foc?g_theme.accent_alt:c);}
   if(slot[i]==OSD_VOLUME&&osd.volume_active){
    const int sx=x+w/2,bot=chip_y-14,th=90,tw=6,v=std::max(0,std::min(100,osd.volume)),fh=th*v/100;
    round_rect(sx-22,bot-th-34,44,th+44,9,0x0A0C18,200,g_theme.white,40);
    rect(sx-tw/2,bot-th,tw,th,0x5A5E70);if(fh>0)rect(sx-tw/2,bot-fh,tw,fh,g_theme.accent);
    const std::string n=std::to_string(v);face_text(sx-face_text_width(n,13,TextFace::Classic)/2,bot-th-28,n,g_theme.white,13,TextFace::Classic);}
   x-=10;}}
 // Progress bar and the times under it.
 // Live TV has nothing to scrub: no track, just the LIVE badge below.
 const int px=57,py=628,pw=1223-57,ph=6;if(!osd.live)round_rect(px,py,pw,ph,3,g_theme.white,41);
 const uint64_t shown=osd.scrubbing?osd.target:osd.position;int fill=0;
 if(!osd.live&&osd.duration)fill=(int)(pw*std::min(1.0,(double)shown/osd.duration));
 if(fill>0)round_rect(px,py,fill,ph,3,g_theme.accent,255);
 // Focused: the knob on the playhead in a soft accent halo (Left / Right scrub).
 if(osd.control==OSD_SCRUB&&!osd.live&&!osd.scrubbing){circle(px+fill,py+ph/2,13,g_theme.accent,70);circle(px+fill,py+ph/2,7,g_theme.white,255);}
 if(osd.scrubbing&&!osd.live){circle(px+fill,py+ph/2,7,g_theme.white,255);std::string at=clock_text(osd.target);int w=face_text_width(at,16,TextFace::Classic)+20,lx=std::max(px,std::min(px+pw-w,px+fill-w/2));round_rect(lx,py-38,w,26,6,0x0A0C18,210,g_theme.accent_alt,200);face_text(lx+10,py-34,at,g_theme.white,16,TextFace::Classic);}
 if(osd.live){int w=face_text_width(T("LIVE"),12,TextFace::Classic)+16;round_rect(px,644,w,18,9,0xD8344A,235);face_text(px+8,646,T("LIVE"),g_theme.white,12,TextFace::Classic);}
 else{face_text(px,644,clock_text(shown),0xE0E0E0,15,TextFace::Classic);if(osd.duration){const uint64_t left=osd.duration>shown?osd.duration-shown:0;std::string rem="-"+clock_text(left),end=ends_at(left);const int rw=face_text_width(rem,15,TextFace::Classic);face_text(px+pw-rw,644,rem,0xE0E0E0,15,TextFace::Classic);face_text(px+pw-rw-20-face_text_width(end,15,TextFace::Classic),644,end,0x9A9EB4,15,TextFace::Classic);}}
 // Hints: what X does, seeking, stop and hide.
 // X does what the focused control does; Left / Right move the focus, or
 // scrub when the bar itself is focused.
 const char* scrub_hint=osd.control==OSD_SCRUB?"Scrub":"Select";
 const char* act=osd.control==OSD_VOLUME?"Volume":osd.control==OSD_PICTURE?"Picture":osd.paused?"Play":"Pause";
 if(osd.volume_active&&!osd.live)draw_hints(57,680,{{'D',"Volume"},{'C',"Done"}});
 else if(osd.live){
  if(!osd.up_next.empty()){const std::string next=std::string(T("NEXT CHANNEL"))+"  ";const std::string name=fit_face(osd.up_next,360,15,TextFace::Classic);const int w=face_text_width(next,12,TextFace::Classic),nw=face_text_width(name,15,TextFace::Classic);
   face_text(1223-nw-w,662,next,g_theme.accent_alt,12,TextFace::Classic);face_text(1223-nw,659,name,0xE0E0E0,15,TextFace::Classic);}
  // Nothing to pause or seek: Up / Down are the volume, X opens Picture.
  if(osd.has_prev||osd.has_next)draw_hints(57,680,{{'D',"Volume"},{'X',"Picture"},{'l',""},{'r',"Channel"},{'C',"Stop"},{'S',"Hide"}});
  else draw_hints(57,680,{{'D',"Volume"},{'X',"Picture"},{'C',"Stop"},{'S',"Hide"}});}
 // A series: L1 / R1 are the previous and next episodes.
 else if(osd.has_prev||osd.has_next){
  if(!osd.up_next.empty()){const std::string next=std::string(T("UP NEXT"))+"  ";const int w=face_text_width(next,12,TextFace::Classic),nw=face_text_width(fit_face(osd.up_next,360,15,TextFace::Classic),15,TextFace::Classic);
   face_text(1223-nw-w,662,next,g_theme.accent_alt,12,TextFace::Classic);face_text(1223-nw,659,fit_face(osd.up_next,360,15,TextFace::Classic),0xE0E0E0,15,TextFace::Classic);}
  if(osd.has_prev&&osd.has_next)draw_hints(57,680,{{'X',act},{'D',scrub_hint},{'L',""},{'R',"Skip 5 min"},{'l',"Previous"},{'r',"Next"},{'C',"Stop"}});
  else if(osd.has_next)draw_hints(57,680,{{'X',act},{'D',scrub_hint},{'L',""},{'R',"Skip 5 min"},{'r',"Next episode"},{'C',"Stop"}});
  else draw_hints(57,680,{{'X',act},{'D',scrub_hint},{'L',""},{'R',"Skip 5 min"},{'l',"Previous episode"},{'C',"Stop"}});}
 else draw_hints(57,680,{{'X',act},{'D',scrub_hint},{'L',""},{'R',"Skip 5 min"},{'C',"Stop"},{'S',"Hide"}});
}
void draw_skip_button(const std::string& label,bool button){
 // Above the bar's chips, so it reads with the bar up or away.
 const int tw=face_text_width(label,18,TextFace::Classic),gw=button?hints_width({{'T',""}})+6:0,w=tw+gw+40,h=44,x=1223-w,y=462;
 round_rect(x,y,w,h,h/2,0x0A0C18,215,button?g_theme.accent_alt:g_theme.white,button?255:50);
 if(button)draw_hints(x+18,y+(h-18)/2,{{'T',""}});
 face_text(x+20+gw,y+(h-18)/2-2,label,g_theme.white,18,TextFace::Classic);
}
void draw_picture_banner(const std::string& state,bool moving){
 constexpr int W=900,H=96,X=(WIDTH-W)/2,Y=40;
 round_rect(X,Y,W,H,12,0x0A0C18,215,g_theme.white,40);
 face_text(X+(W-face_text_width(state,19,TextFace::Classic))/2,Y+18,state,g_theme.white,19,TextFace::Classic);
 const int hw=hints_width({{'D',moving?"Move":"Size"},{'r',moving?"Size":"Move"},{'S',"Fit / Fill / Stretch"},{'T',"Hold: Reset"},{'C',"Done"}});
 draw_hints(X+(W-hw)/2,Y+58,{{'D',moving?"Move":"Size"},{'r',moving?"Size":"Move"},{'S',"Fit / Fill / Stretch"},{'T',"Hold: Reset"},{'C',"Done"}});
}
void draw_volume_banner(int volume){
 constexpr int W=520,H=80,X=(WIDTH-W)/2,Y=40,BX=X+40,BW=W-80;
 round_rect(X,Y,W,H,12,0x0A0C18,215,g_theme.white,40);
 const std::string label=std::string(T("Volume"))+"   "+std::to_string(volume)+"%";
 face_text(X+(W-face_text_width(label,19,TextFace::Classic))/2,Y+16,label,g_theme.white,19,TextFace::Classic);
 round_rect(BX,Y+52,BW,6,3,g_theme.white,41);
 const int fill=BW*std::max(0,std::min(100,volume))/100;if(fill>0)round_rect(BX,Y+52,fill,6,3,g_theme.accent,255);
 circle(BX+fill,Y+55,7,g_theme.white,255);
}

// ---- music: Now Playing (PS3 music_screen.cpp) -----------------------------------
static std::vector<uint32_t> music_bg;static std::string music_bg_key;
// The cover shrunk to a few dozen pixels and stretched back: a dark wash of
// its colours behind everything. Composed once per cover.
static void music_background(const std::string& key){
 const std::string want=!key.empty()&&art_state(key)==1?key:"";
 if(music_bg_key==want&&music_bg.size()==(size_t)WIDTH*HEIGHT){memcpy(framebuffer(),music_bg.data(),music_bg.size()*4);return;}
 constexpr int SW=48,SH=27;std::vector<uint32_t> small;
 if(!want.empty()&&draw_art_cover(want,0,0,SW,SH,false)){auto fb=framebuffer();for(int y=0;y<SH;++y)small.insert(small.end(),fb+y*WIDTH,fb+y*WIDTH+SW);}
 background(0,HEIGHT);
 if(!small.empty()){auto fb=framebuffer();
  for(int y=0;y<HEIGHT;++y){const float fy=std::max(0.0f,std::min(SH-1.001f,(y+0.5f)*SH/HEIGHT-0.5f));const int y0=(int)fy;const float ty=fy-y0;
   // Darker toward the bottom, where the progress bar and hints sit.
   const int keep=std::max(40,110-y*70/HEIGHT);
   for(int x=0;x<WIDTH;++x){const float fx=std::max(0.0f,std::min(SW-1.001f,(x+0.5f)*SW/WIDTH-0.5f));const int x0=(int)fx;const float tx=fx-x0;
    const uint32_t* p=&small[y0*SW+x0];uint32_t out=0;
    for(int s=0;s<24;s+=8){const float top=((p[0]>>s)&255)*(1-tx)+((p[1]>>s)&255)*tx,bottom=((p[SW]>>s)&255)*(1-tx)+((p[SW+1]>>s)&255)*tx;out|=(uint32_t)(top*(1-ty)+bottom*ty)<<s;}
    fb[y*WIDTH+x]=0xff000000|mix(fb[y*WIDTH+x]&0xffffff,out,keep);}}}
 music_bg.assign(framebuffer(),framebuffer()+WIDTH*HEIGHT);music_bg_key=want;
}
// ---- visualizers -----------------------------------------------------------------
const char* music_viz_name(int viz){static const char* const names[VIZ_COUNT]={"Bars","Aurora","Halo","Scope","Off"};return names[std::max(0,std::min(VIZ_COUNT-1,viz))];}
// One pixel `a` of 256 toward `c`.
static inline void glow_px(uint32_t& d,uint32_t c,uint32_t a){
 if(!a)return;if(a>256)a=256;const uint32_t k=256-a;
 d=0xff000000|((((d&0xff00ff)*k+(c&0xff00ff)*a)>>8)&0xff00ff)|((((d&0x00ff00)*k+(c&0x00ff00)*a)>>8)&0x00ff00);
}
static float band_level(const float* bands,int from,int to){float s=0;for(int i=from;i<to;++i)s+=bands[i];return to>from?std::min(1.0f,s/(to-from)):0;}
// Aurora: three soft ribbons drifting across the whole screen, behind the
// cover; the bass, the mids and the highs each swell one of them.
static void viz_aurora(const float* bands,int count){
 const float t=mono_us()/1e6f,lv[3]={band_level(bands,0,count/6),band_level(bands,count/6,count/2),band_level(bands,count/2,count)};
 const uint32_t colour[3]={g_theme.accent,g_theme.accent_alt,mix(g_theme.accent,g_theme.white,90)};const float base[3]={250,205,290};
 auto fb=framebuffer();
 for(int r=0;r<3;++r){const float th=14+70*lv[r],amp=30+90*lv[r],peak=50+130*lv[r];
  for(int x=0;x+1<WIDTH;x+=2){const float fx=x/(float)WIDTH;
   const float yc=base[r]+amp*(0.6f*sinf(fx*6.2832f*(1.1f+0.4f*r)+t*(0.6f+0.3f*r))+0.4f*sinf(fx*6.2832f*(2.6f-0.5f*r)-t*(1.0f+0.25f*r)));
   const int y0=std::max(0,(int)(yc-th)),y1=std::min(HEIGHT-1,(int)(yc+th));
   for(int y=y0;y<=y1;++y){const float d=(y-yc)/th,f=1-d*d;if(f<=0)continue;const uint32_t a=(uint32_t)(f*f*peak);uint32_t* p=fb+y*WIDTH+x;glow_px(p[0],colour[r],a);glow_px(p[1],colour[r],a);}}}
}
// Halo: the spectrum as rays all round the cover (drawn as a disc), mirrored
// left and right with the bass at the top; the bass makes the ring glow.
static void viz_halo(const float* bands,int count,int cx,int cy,int r0){
 auto fb=framebuffer();const float bass=band_level(bands,0,std::max(1,count/7));
 // The glow ring.
 const int outer=r0+26;
 for(int y=cy-outer;y<=cy+outer;++y){if(y<0||y>=HEIGHT)continue;const int dy=y-cy;
  for(int x=cx-outer;x<=cx+outer;++x){if(x<0||x>=WIDTH)continue;const int dx=x-cx,d2=dx*dx+dy*dy;if(d2<r0*r0||d2>outer*outer)continue;
   const float f=1-(sqrtf((float)d2)-r0)/26.0f;glow_px(fb[y*WIDTH+x],g_theme.accent_alt,(uint32_t)(f*f*(40+170*bass)));}}
 const int n=count*2;
 for(int i=0;i<n;++i){const int b=i<count?i:n-1-i;const float lv=std::max(0.0f,std::min(1.0f,bands[b]));
  const float ang=-1.5707963f+(i+0.5f)*6.2831853f/n,c=cosf(ang),s=sinf(ang),len=6+lv*92;
  const uint32_t colour=mix(g_theme.accent,g_theme.accent_alt,b*255/std::max(1,count-1));
  // Each ray three pixels wide across its direction, brightest down the middle.
  for(float d=4;d<len;d+=1.0f){const uint32_t a=(uint32_t)(230*(1-0.75f*d/len));
   for(int k=-1;k<=1;++k){const int px=(int)(cx+c*(r0+d)-s*k),py=(int)(cy+s*(r0+d)+c*k);if(px<0||py<0||px>=WIDTH||py>=HEIGHT)continue;glow_px(fb[py*WIDTH+px],colour,k?a/2:a);}}}
}
// Scope: the sound itself, a bright trace with a soft glow either side.
static void viz_scope(const float* wave,int n,int x0,int w,int cy,int amp){
 auto fb=framebuffer();int prev=cy;const uint32_t core=mix(g_theme.accent_alt,g_theme.white,150);
 // A soft limit (tanh) instead of a hard one: loud music rounds off rather than flattening.
 for(int x=0;x<w;++x){const float s=wave&&n>0?tanhf(wave[(int64_t)x*n/w]*1.3f):0;const int y=cy-(int)(s*amp);
  const int lo=std::min(prev,y),hi=std::max(prev,y);
  for(int yy=lo-9;yy<=hi+9;++yy){if(yy<0||yy>=HEIGHT)continue;const int dist=yy<lo?lo-yy:yy>hi?yy-hi:0;
   if(dist<=1)glow_px(fb[yy*WIDTH+x0+x],core,dist?200:256);else{const float f=1-dist/10.0f;glow_px(fb[yy*WIDTH+x0+x],g_theme.accent,(uint32_t)(f*f*110));}}
  prev=y;}
}
void draw_music_screen(const PlayerOsd& osd,const float* bands,int count,const float* wave,int wave_n){
 music_background(osd.art);
 constexpr int CX=96,CY=104,CS=380,TX=CX+CS+60,TR=1223,TW=TR-TX;
 if(osd.viz==VIZ_AURORA)viz_aurora(bands,count);
 // The cover, large, at the left; with Halo a disc ringed by the spectrum.
 if(osd.viz==VIZ_HALO){constexpr int D=300;const int cx=CX+CS/2,cy=CY+CS/2;viz_halo(bands,count,cx,cy,D/2+8);
  circle(cx,cy,D/2,g_theme.thumb_dim,255);if(osd.art.empty()||!draw_art_circle(osd.art,cx-D/2,cy-D/2,D))draw_brand_mark(cx-60,cy-60,120);ring(cx,cy,D/2,g_theme.hairline);}
 else{
  blend_rect(CX+6,CY+10,CS,CS,0,110);rect(CX,CY,CS,CS,g_theme.thumb_dim);
  if(osd.art.empty()||!draw_art_cover(osd.art,CX,CY,CS,CS))draw_brand_mark(CX+(CS-120)/2,CY+(CS-120)/2,120);
  outline(CX-1,CY-1,CS+2,CS+2,g_theme.hairline,1);
 }
 constexpr int BASE=292,MAX_H=176,GAP=6;
 if(osd.viz==VIZ_SCOPE)viz_scope(wave,wave_n,TX,TW,BASE-MAX_H/2,MAX_H/2);
 // Bars over the title: gel capsules in the accent, a brighter core, a lit
 // cap riding the level and a faint reflection under the baseline.
 if(osd.viz==VIZ_BARS&&count>0){const int bw=std::max(4,(TW-GAP*(count-1))/count),r=bw/2;
  for(int i=0;i<count;++i){const float lv=std::max(0.0f,std::min(1.0f,bands[i]));const int h=bw+(int)(lv*(MAX_H-bw)),bx=TX+i*(bw+GAP),by=BASE-h;
   round_rect(bx,by,bw,h,r,g_theme.accent,130);
   if(bw>=6&&h>bw)round_rect(bx+bw/4,by+r,bw-2*(bw/4),h-r,bw/4,g_theme.accent_alt,(int)(70+90*lv));
   round_rect(bx,by,bw,bw,r,mix(g_theme.accent_alt,g_theme.white,110),(int)(150+100*lv));
   if(h/3>2)round_rect(bx,BASE+4,bw,h/3,r,g_theme.accent,26);}}
 // Title, artist in the accent, then album and year.
 int y=BASE+76;face_text(TX,y,fit_face(osd.title,osd.tracks>1?TW-150:TW,34,TextFace::Classic),g_theme.white,34,TextFace::Classic);y+=48;
 if(!osd.subtitle.empty()){face_text(TX,y,fit_face(osd.subtitle,TW,20,TextFace::Classic),g_theme.accent_alt,20,TextFace::Classic);y+=30;}
 // The album position sits at the right of the title.
 if(osd.tracks>1){const std::string n=TF("Track %d of %d",osd.track,osd.tracks);face_text(TR-face_text_width(n,15,TextFace::Classic),BASE+90,n,g_theme.text_faint,15,TextFace::Classic);}
 if(!osd.album.empty())face_text(TX,y,fit_face(osd.album,TW,17,TextFace::Classic),g_theme.text_dim,17,TextFace::Classic);
 // Progress, times and when it ends.
 const int py=CY+CS-6,ph=6;round_rect(TX,py,TW,ph,3,g_theme.white,41);
 const uint64_t shown=osd.scrubbing?osd.target:osd.position;int fill=osd.duration?(int)(TW*std::min(1.0,(double)shown/osd.duration)):0;
 if(fill>0)round_rect(TX,py,fill,ph,3,g_theme.accent,255);
 if(osd.scrubbing)circle(TX+fill,py+ph/2,7,g_theme.white,255);
 face_text(TX,py+18,clock_text(shown),0xE0E0E0,15,TextFace::Classic);
 if(osd.paused){const int w=face_text_width(T("PAUSED"),13,TextFace::Classic)+16;round_rect(TX,py-34,w,20,10,g_theme.accent,220);face_text(TX+8,py-31,T("PAUSED"),g_theme.white,13,TextFace::Classic);}
 if(osd.duration){const uint64_t left=osd.duration>shown?osd.duration-shown:0;std::string rem="-"+clock_text(left),end=ends_at(left);const int rw=face_text_width(rem,15,TextFace::Classic);
  face_text(TR-rw,py+18,rem,0xE0E0E0,15,TextFace::Classic);face_text(TR-rw-20-face_text_width(end,15,TextFace::Classic),py+18,end,0x9A9EB4,15,TextFace::Classic);}
 // What plays when this one ends.
 if(!osd.up_next.empty()){const int w=face_text_width(T("UP NEXT"),12,TextFace::Classic);face_text(TX,py+52,T("UP NEXT"),g_theme.accent_alt,12,TextFace::Classic);face_text(TX+w+12,py+49,fit_face(osd.up_next,TW-w-12,17,TextFace::Classic),g_theme.text,17,TextFace::Classic);}
 // Which visualizer Square has just chosen.
 if(osd.viz_toast){const std::string label=std::string(T("Visualizer"))+": "+T(music_viz_name(osd.viz));const int w=face_text_width(label,18,TextFace::Classic)+40;
  round_rect((WIDTH-w)/2,30,w,40,20,0x0A0C18,215,g_theme.accent,120);face_text((WIDTH-face_text_width(label,18,TextFace::Classic))/2,40,label,g_theme.white,18,TextFace::Classic);}
 if(osd.tracks>1)draw_hints(57,680,{{'X',osd.paused?"Play":"Pause"},{'D',"Seek / Volume"},{'l',"Previous"},{'r',"Next"},{'S',"Visualizer"},{'C',"Stop"}});
 else draw_hints(57,680,{{'X',osd.paused?"Play":"Pause"},{'D',"Seek / Volume"},{'L',""},{'R',"Skip 5 min"},{'S',"Visualizer"},{'C',"Stop"}});
}

// ---- settings (PS3 ui_settings.cpp) ------------------------------------------------
constexpr int SET_W=860,SET_X=(WIDTH-SET_W)/2,SET_TILE=40;
// Each category's glyph, drawn in its tile.
static void settings_icon(SettingsIcon icon,int x,int y,int s,uint32_t color){
 const int cx=x+s/2,cy=y+s/2;
 switch(icon){
 case SICON_PLAYBACK:{const int th=s*11/20,tw=th*7/8,tx=cx-tw/2+s/20,ty=cy-th/2;for(int r=0;r<th;++r){const float f=(float)r/(th-1);const int w=(int)((1.0f-fabsf(2.0f*f-1.0f))*tw+0.5f);if(w>0)rect(tx,ty+r,w,1,color);}break;}
 case SICON_DISPLAY:{const int w=s*13/20,h=s*9/20,sx=cx-w/2,sy=y+s*6/20;outline(sx,sy,w,h,color,2);rect(cx-1,sy+h,3,s/10,color);rect(cx-s/6,sy+h+s/10,s/3,2,color);break;}
 case SICON_ACCOUNT:{circle(cx,y+s*8/20,s*3/20,color,255);for(int dy=0;dy<s*5/20;++dy){const int r=s*6/20,yy=dy-r,half=(int)sqrtf((float)std::max(0,r*r-yy*yy));blend_rect(cx-half,y+s*12/20+dy,2*half,1,color,255);}break;}
 default:{const int r=s*6/20;for(int k=0;k<8;++k){const float a=k*0.785398f;circle(cx+(int)lroundf(cosf(a)*r),cy+(int)lroundf(sinf(a)*r),s/12,color,255);}circle(cx,cy,r,color,255);circle(cx,cy,s/9,g_theme.accent_deep,255);break;}
 }
}
static void settings_tile(SettingsIcon icon,int x,int y,bool lit){round_rect(x,y,SET_TILE,SET_TILE,8,lit?g_theme.accent_deep:g_theme.thumb_dim,255);settings_icon(icon,x+6,y+6,SET_TILE-12,lit?g_theme.white:g_theme.text_dim);}
void draw_settings_categories(const std::vector<SettingsCategoryView>& categories,int selected,const std::string& account,const std::string& footer){
 app_header("","Settings");
 face_text(SET_X,100,fit_face(account,SET_W,14,TextFace::Classic),g_theme.text_faint,14,TextFace::Classic);
 constexpr int TOP=128,ROW=74,GAP=8;
 for(int i=0;i<(int)categories.size();++i){const auto& c=categories[i];const bool on=i==selected;const int y=TOP+i*(ROW+GAP);
  rect(SET_X,y,SET_W,ROW,on?g_theme.panel_hi:g_theme.panel);if(on)rect(SET_X-5,y,3,ROW,g_theme.accent);
  settings_tile(c.icon,SET_X+16,y+(ROW-SET_TILE)/2,on);
  const int tx=SET_X+16+SET_TILE+20,tw=SET_W-(tx-SET_X)-60;
  face_text(tx,y+12,fit_face(c.label,tw,21,TextFace::Classic),on?g_theme.white:g_theme.text,21,TextFace::Classic);
  face_text(tx,y+42,fit_face(c.summary,tw,14,TextFace::Classic),g_theme.text_faint,14,TextFace::Classic);
  face_text(SET_X+SET_W-36,y+(ROW-28)/2,"\xE2\x80\xBA",on?g_theme.accent:g_theme.text_faint,28,TextFace::Classic);}
 face_text((WIDTH-face_text_width(footer,13,TextFace::Classic))/2,618,footer,g_theme.text_faint,13,TextFace::Classic);
 hint_bar({{'D',"Select"},{'X',"Open"},{'C',"Back"},{'l',"Search"},{'r',"Home"}});
}
void draw_settings_rows(const SettingsCategoryView& category,const std::vector<SettingsRowView>& rows,int selected){
 app_header("","Settings");
 settings_tile(category.icon,SET_X+16,92,true);face_text(SET_X+16+SET_TILE+20,98,category.label,g_theme.white,24,TextFace::Classic);
 rect(SET_X,146,SET_W,1,g_theme.hairline);
 constexpr int TOP=160,ROW=46,GAP=6,DESC_Y=548,DESC_H=78;
 // Rows scroll to keep the highlighted one above the description.
 const int fit=(DESC_Y-8-TOP)/(ROW+GAP),first=std::max(0,std::min(selected-fit+1,(int)rows.size()-fit));
 for(int k=first;k<(int)rows.size()&&k<first+fit;++k){const auto& r=rows[k];const bool on=k==selected;const int y=TOP+(k-first)*(ROW+GAP);
  if(on){rect(SET_X,y,SET_W,ROW,g_theme.panel_hi);rect(SET_X-5,y,3,ROW,g_theme.accent);}
  // The row being moved: lifted, in the accent, with arrows above and below its name.
  if(r.held){outline(SET_X,y,SET_W,ROW,g_theme.accent_alt,2);face_text(SET_X+SET_W/2+40,y+11,"\xE2\x86\x95",g_theme.accent_alt,20,TextFace::Classic);}
  const uint32_t ink=r.dim?g_theme.text_faint:on?g_theme.white:g_theme.text;
  face_text(SET_X+20,y+13,fit_face(r.label,SET_W/2,18,TextFace::Classic),ink,18,TextFace::Classic);
  if(r.action)face_text(SET_X+SET_W-36,y+(ROW-28)/2,"\xE2\x80\xBA",on?g_theme.accent:g_theme.text_faint,28,TextFace::Classic);
  else{const std::string v=fit_face(r.value,SET_W/2-40,17,TextFace::Classic);const uint32_t vc=r.dim?g_theme.text_faint:r.lit?g_theme.accent_alt:g_theme.text_dim;
   // Left / Right change it: arrows either side of the highlighted value.
   const bool arrows=on&&!r.dim&&!r.reorder;const int vw=face_text_width(v,17,TextFace::Classic),vx=SET_X+SET_W-(arrows?44:20)-vw;face_text(vx,y+14,v,vc,17,TextFace::Classic);
   if(arrows){face_text(vx-22,y+11,"\xE2\x80\xB9",g_theme.text_dim,22,TextFace::Classic);face_text(SET_X+SET_W-30,y+11,"\xE2\x80\xBA",g_theme.text_dim,22,TextFace::Classic);}}}
 // What the highlighted setting does, in two lines.
 rect(SET_X,DESC_Y,SET_W,DESC_H,g_theme.panel);outline(SET_X,DESC_Y,SET_W,DESC_H,g_theme.hairline,1);rect(SET_X,DESC_Y,3,DESC_H,g_theme.accent);
 if(selected>=0&&selected<(int)rows.size()){const std::string& d=rows[selected].description;const size_t nl=d.find('\n');
  face_text(SET_X+22,DESC_Y+16,fit_face(d.substr(0,nl),SET_W-44,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic);
  if(nl!=std::string::npos)face_text(SET_X+22,DESC_Y+42,fit_face(d.substr(nl+1),SET_W-44,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic);}
 const bool action=selected>=0&&selected<(int)rows.size()&&rows[selected].action;
 const SettingsRowView* sel=selected>=0&&selected<(int)rows.size()?&rows[selected]:nullptr;
 if(sel&&sel->reorder&&sel->held)hint_bar({{'D',"Move"},{'X',"Put down"},{'C',"Done"}});
 else if(sel&&sel->reorder)hint_bar({{'D',"Select"},{'X',"Move"},{'T',"Default order"},{'C',"Done"}});
 else if(action)hint_bar({{'D',"Select"},{'X',"Open"},{'C',"Back"}});else hint_bar({{'D',"Select / change"},{'X',"Change"},{'C',"Back"}});
}
void draw_confirm(const std::string& question,const std::string& detail,const char* yes){
 blend_rect(0,0,WIDTH,HEIGHT,0,150);constexpr int W=560,H=150,X=(WIDTH-W)/2,Y=250;
 round_rect(X,Y,W,H,10,g_theme.panel,255,g_theme.hairline,255);
 face_text(X+(W-face_text_width(question,21,TextFace::Classic))/2,Y+28,question,g_theme.text,21,TextFace::Classic);
 const std::string d=fit_face(detail,W-40,15,TextFace::Classic);face_text(X+(W-face_text_width(d,15,TextFace::Classic))/2,Y+66,d,g_theme.text_dim,15,TextFace::Classic);
 const int hw=hints_width({{'X',yes},{'C',"Cancel"}});draw_hints(X+(W-hw)/2,Y+H-36,{{'X',yes},{'C',"Cancel"}});
}

void draw_update_panel(const std::string& title,const std::string& detail,float progress,const char* yes,const char* no){
 blend_rect(0,0,WIDTH,HEIGHT,0,170);constexpr int W=700,H=220,X=(WIDTH-W)/2,Y=230;
 round_rect(X,Y,W,H,10,g_theme.panel,255,g_theme.hairline,255);rect(X,Y,W,3,g_theme.accent);
 face_text(X+(W-face_text_width(title,22,TextFace::Classic))/2,Y+26,title,g_theme.text,22,TextFace::Classic);
 const size_t nl=detail.find('\n');const std::string a=detail.substr(0,nl),b=nl==std::string::npos?"":detail.substr(nl+1);
 face_text(X+(W-face_text_width(fit_face(a,W-48,15,TextFace::Classic),15,TextFace::Classic))/2,Y+70,fit_face(a,W-48,15,TextFace::Classic),g_theme.text_dim,15,TextFace::Classic);
 if(!b.empty())face_text(X+(W-face_text_width(fit_face(b,W-48,15,TextFace::Classic),15,TextFace::Classic))/2,Y+94,fit_face(b,W-48,15,TextFace::Classic),g_theme.text_dim,15,TextFace::Classic);
 if(progress>=0){const int bx=X+48,bw=W-96,by=Y+132;round_rect(bx,by,bw,8,4,g_theme.white,41);const int fill=(int)(bw*std::min(1.0f,progress));if(fill>0)round_rect(bx,by,fill,8,4,g_theme.accent,255);}
 if(yes&&no){const int hw=hints_width({{'X',yes},{'C',no}});draw_hints(X+(W-hw)/2,Y+H-40,{{'X',yes},{'C',no}});}
 else if(yes||no){const Hint h{yes?'X':'C',yes?yes:no};const int hw=hints_width({h});draw_hints(X+(W-hw)/2,Y+H-40,{h});}
}

// ---- about (PS3 Settings > About) -------------------------------------------------
namespace {
enum AboutKind { A_HEAD, A_VER, A_ITEM, A_NAME, A_TEXT, A_GAP };
struct AboutLine { AboutKind kind; const char* a; const char* b; };
// Updates: the PS4 releases, newest first. Add each new release at the top.
const AboutLine about_updates[]={
 {A_VER,"1.11",nullptr},
 {A_ITEM,"TV shows play on to the next episode, across seasons; L1 / R1 skip episodes.",nullptr},
 {A_ITEM,"Skip Intros and Skip Credits, with markers from the server (Intro Skipper).",nullptr},
 {A_ITEM,"Player bar as on the PS3: Left / Right scrub; Up for Volume and Picture.",nullptr},
 {A_ITEM,"Live TV: L1 / R1 change channel, a guide on the PS4, reconnects by itself.",nullptr},
 {A_ITEM,"Quick peek shows the cast; Home Order; Share Sign-in.",nullptr},
 {A_ITEM,"The controller's light bar takes the theme's colour.",nullptr},
 {A_ITEM,"Fixes: empty albums, films listed twice, theme cycling, Live TV crashes.",nullptr},
 {A_VER,"1.10",nullptr},
 {A_ITEM,"Starts in about 4 seconds; every screen and animation is smooth.",nullptr},
 {A_ITEM,"Direct play: H264 in MP4, MKV and MOV, and most music, start in 1-3 seconds.",nullptr},
 {A_ITEM,"Smoother video, and 1080p playback (Settings > Playback, experimental).",nullptr},
 {A_ITEM,"Live TV channels play straight from their source: faster and sharper.",nullptr},
 {A_ITEM,"The PS3 buffering screen, with the ring, the mark and progress.",nullptr},
 {A_ITEM,"L1 while watching: picture size, position and Fit / Fill / Stretch.",nullptr},
 {A_ITEM,"Music: Now Playing, albums play through (L1 / R1 skip), four visualizers.",nullptr},
 {A_ITEM,"The clock, the date and your picture in the header; \"Ends at\" times.",nullptr},
 {A_ITEM,"Languages: English, Japanese, Portuguese, German, French and Spanish.",nullptr},
 {A_ITEM,"Settings in groups, Software Update, and a Debug Logging switch.",nullptr},
 {A_ITEM,"Artwork shimmers while it loads; a smoother quick peek; a new app icon.",nullptr},
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
   case A_HEAD:face_text(x0,y+12,T(l.a),g_theme.accent_alt,18,TextFace::Classic);break;
   case A_VER:face_text(x0,y+8,TF("Version %s",l.a),g_theme.accent_alt,18,TextFace::Classic);break;
   case A_ITEM:face_text(x0+6,y,"\xC2\xB7",g_theme.accent,16,TextFace::Classic);face_text(x0+22,y,fit_face(l.a,col_w-22,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic);break;
   case A_NAME:{face_text(x0,y,l.a,g_theme.text,17,TextFace::Classic);if(l.b){int bw=face_text_width(l.b,15,TextFace::Classic);face_text(x0+col_w-bw,y+1,l.b,g_theme.text_dim,15,TextFace::Classic);}break;}
   case A_TEXT:face_text(x0,y,fit_face(T(l.a),col_w,16,TextFace::Classic),g_theme.text_dim,16,TextFace::Classic);break;
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
 int widths[3],total=0;for(int i=0;i<3;++i){widths[i]=face_text_width(T(about_sections[i].label),18,TextFace::Classic);total+=widths[i]+(i?44:0);}
 int x=(WIDTH-total)/2;for(int i=0;i<3;++i){bool on=i==section;face_text(x,140,T(about_sections[i].label),on?g_theme.white:g_theme.text_faint,18,TextFace::Classic);if(on)rect(x,166,widths[i],3,g_theme.accent);x+=widths[i]+44;}
 background(ABOUT_BOTTOM,650);
 hint_bar({{'l',""},{'r',"Section"},{'D',"Scroll"},{'C',"Back"}});
}
