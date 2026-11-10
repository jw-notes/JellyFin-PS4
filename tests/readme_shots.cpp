// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
// The README's screenshots, rendered on the host by the app's own drawing code.
// Titles, years, runtimes, ratings, directors, cast and episode names are real;
// the synopses are short descriptions of our own, and every image -- posters,
// backdrops, stills, album covers, portraits -- is generated here (no studio
// artwork). Live TV uses the channel names and groups of an M3U playlist given
// as the first argument. Writes build/readme/*.bmp.
#include "app.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "screenshot.h"
#include "buffering.h"
#include "music_viz.h"
#include "language.h"
#include "boot_anim.h"
#include "screensaver.h"
#include "trickplay.h"
#include "livetv.h"
#include "api_livetv.h"
#include "stb_image.h"
#include <cctype>
#include <sys/stat.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
void art_inject(const std::string& key,int w,int h,const std::vector<uint8_t>& rgba);
void screensaver_preview(int index);
void livetv_preview(const std::vector<JFChannel>& channels,int selected,bool show_guide);

static std::vector<uint32_t> pixels(WIDTH*HEIGHT);
bool keyboard(const std::string&,std::string&,bool,KeyboardType){return false;}
uint32_t* framebuffer(){return pixels.data();}void present(){usleep(16000);}uint32_t buttons(){return 0;}
static void capture(const char* name){std::string path=std::string("build/readme/")+name+".bmp";FILE* f=fopen(path.c_str(),"wb");assert(f&&write_frame_bmp(f,pixels.data(),WIDTH,HEIGHT,WIDTH));fclose(f);printf("  %s\n",path.c_str());}

// ---- generated artwork -------------------------------------------------------------
static uint32_t hsv(float h,float s,float v){h=fmodf(h,1.0f)*6;const int i=(int)h;const float f=h-i,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));float r,g,b;
 switch(i){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t;break;case 3:r=p;g=q;b=v;break;case 4:r=t;g=p;b=v;break;default:r=v;g=p;b=q;}
 return ((uint32_t)(r*255)<<16)|((uint32_t)(g*255)<<8)|(uint32_t)(b*255);}
static uint32_t lerp(uint32_t a,uint32_t b,float t){auto c=[&](int s){return (uint32_t)(((a>>s)&255)*(1-t)+((b>>s)&255)*t)<<s;};return c(16)|c(8)|c(0);}
static void soft_disc(int w,int h,float cx,float cy,float r,uint32_t colour,float alpha){
 for(int y=std::max(0,(int)(cy-r));y<std::min(h,(int)(cy+r));++y)for(int x=std::max(0,(int)(cx-r));x<std::min(w,(int)(cx+r));++x){
  const float d=sqrtf((x-cx)*(x-cx)+(y-cy)*(y-cy))/r;if(d>=1)continue;uint32_t& p=pixels[y*WIDTH+x];p=0xff000000|lerp(p&0xffffff,colour,alpha*(1-d)*(1-d));}}
static std::vector<uint8_t> grab(int w,int h){std::vector<uint8_t> out((size_t)w*h*4);for(int y=0;y<h;++y)for(int x=0;x<w;++x){const uint32_t p=pixels[y*WIDTH+x];uint8_t* o=&out[((size_t)y*w+x)*4];o[0]=(p>>16)&255;o[1]=(p>>8)&255;o[2]=p&255;o[3]=255;}return out;}
static unsigned seed_of(const std::string& s){unsigned h=2166136261u;for(char c:s)h=(h^(unsigned char)c)*16777619u;return h;}
// A scene: two hues, a horizon of light and a few glowing shapes. No text.
static void scene(int w,int h,unsigned seed){
 const float hue=(seed%1000)/1000.0f,hue2=hue+0.08f+((seed>>10)%100)/600.0f;const uint32_t top=hsv(hue,0.65f,0.30f),mid=hsv(hue2,0.55f,0.62f),bottom=hsv(hue,0.70f,0.10f);
 const float horizon=0.45f+((seed>>4)%20)/100.0f;
 for(int y=0;y<h;++y){const float t=(float)y/h;const uint32_t c=t<horizon?lerp(top,mid,t/horizon):lerp(mid,bottom,(t-horizon)/(1-horizon));for(int x=0;x<w;++x)pixels[y*WIDTH+x]=0xff000000|c;}
 for(int k=0;k<4;++k){const unsigned s=seed*(k+3)*2654435761u;soft_disc(w,h,(s%997)/997.0f*w,(0.15f+((s>>12)%60)/100.0f)*h,(0.12f+((s>>20)%30)/100.0f)*w,hsv(hue+0.1f*k,0.4f,0.95f),0.55f);}
}
static void title_text(int w,int h,const std::string& title,const std::string& sub,int top){
 int size=std::max(16,w/7);while(size>16&&face_text_width(title,size,TextFace::Poster)>w-28)size-=2;
 const int y=top>=0?top:h-size-(sub.empty()?26:50),x=(w-face_text_width(title,size,TextFace::Poster))/2;
 face_text(x+2,y+2,title,0x000000,size,TextFace::Poster,w);face_text(x,y,title,0xFFFFFF,size,TextFace::Poster,w);
 if(!sub.empty())face_text((w-face_text_width(sub,15,TextFace::Classic))/2,y+size+8,sub,0xE6E6F0,15,TextFace::Classic);}
static void poster(const std::string& key,const std::string& title,const std::string& sub){scene(320,480,seed_of(title));title_text(320,480,title,sub,-1);art_inject(key,320,480,grab(320,480));}
static void backdrop(const std::string& key,const std::string& title,int w=1280,int h=720){scene(w,h,seed_of(title+"#scene"));art_inject(key,w,h,grab(w,h));}
static void album(const std::string& key,const std::string& title,const std::string& artist){
 const unsigned s=seed_of(title);for(int y=0;y<500;++y)for(int x=0;x<500;++x)pixels[y*WIDTH+x]=0xff000000|lerp(hsv((s%1000)/1000.0f,0.6f,0.25f),hsv((s%1000)/1000.0f+0.3f,0.7f,0.8f),(x+y)/1000.0f);
 for(int k=0;k<3;++k)soft_disc(500,500,250+120*cosf(k*2.1f+s%7),250+120*sinf(k*2.1f+s%5),140,hsv((s%1000)/1000.0f+0.15f*k,0.5f,1.0f),0.5f);
 title_text(500,500,title,artist,330);art_inject(key,500,500,grab(500,500));}
static void portrait(const std::string& key,const std::string& name){
 const unsigned s=seed_of(name);for(int y=0;y<200;++y)for(int x=0;x<200;++x)pixels[y*WIDTH+x]=0xff000000|lerp(hsv((s%1000)/1000.0f,0.35f,0.55f),hsv((s%1000)/1000.0f,0.45f,0.25f),y/200.0f);
 soft_disc(200,200,100,78,46,hsv((s%1000)/1000.0f+0.05f,0.2f,0.95f),0.8f);soft_disc(200,200,100,210,90,hsv((s%1000)/1000.0f+0.05f,0.2f,0.95f),0.7f);
 art_inject(key,200,200,grab(200,200));}

// ---- the library ---------------------------------------------------------------------
struct Film{const char* id,*name;int year;const char* rating;int minutes;const char* score,*genres,*director,*overview;std::vector<std::pair<const char*,const char*>> cast;};
// Every work here has freely licensed artwork on Wikimedia Commons
// (tools/fetch_free_art.py): Blender Foundation open movies under Creative
// Commons, and public-domain films, TV episodes and classical works.
static const std::vector<Film> films={
 {"f1","Charade",1963,"NR",113,"7.8","Comedy, Mystery, Romance","Stanley Donen","In Paris, a young widow is pursued by three men after the fortune her husband stole, and helped by a charming stranger whose name keeps changing.",
  {{"Cary Grant","Peter Joshua"},{"Audrey Hepburn","Regina Lampert"},{"Walter Matthau","Hamilton Bartholomew"},{"James Coburn","Tex Panthollow"},{"George Kennedy","Herman Scobie"},{"Ned Glass","Leopold W. Gideon"}}},
 {"f2","Big Buck Bunny",2008,"",10,"6.4","Animation, Comedy","Sacha Goedegebure","A gentle giant of a rabbit takes a hilarious revenge on three rodents who bully the forest's smaller creatures.",{}},
 {"f3","Sintel",2010,"",15,"7.4","Animation, Fantasy","Colin Levy","A lonely girl crosses a harsh world in search of the baby dragon she raised and lost.",{}},
 {"f4","Tears of Steel",2012,"",12,"6.0","Science Fiction","Ian Hubert","In a future Amsterdam, a group of scientists try to save the world by revisiting a painful breakup.",{}},
 {"f5","Night of the Living Dead",1968,"NR",96,"7.8","Horror","George A. Romero","Strangers barricade themselves in a farmhouse as the dead rise and close in around them.",{}},
 {"f6","The Phantom of the Opera",1925,"NR",93,"7.6","Horror, Drama","Rupert Julian","A disfigured genius haunting the Paris Opera House falls for a young soprano and will stop at nothing to make her a star.",{}},
 {"f7","Metropolis",1927,"NR",153,"8.3","Science Fiction, Drama","Fritz Lang","In a towering city of the future, the son of its ruler takes the side of the workers toiling below.",{}},
 {"f8","The General",1926,"NR",78,"8.1","Comedy, Action","Clyde Bruckman, Buster Keaton","A train engineer chases the spies who stole his locomotive, and his sweetheart with it.",{}},
 {"f9","His Girl Friday",1940,"NR",92,"7.8","Comedy, Romance","Howard Hawks","A newspaper editor schemes to keep his star reporter, who is also his ex-wife, from remarrying.",{}},
 {"f10","Elephants Dream",2006,"",11,"6.5","Animation, Science Fiction","Bassam Kurdali","Two strange characters explore a vast machine that one of them may have imagined.",{}},
 {"f11","Cosmos Laundromat",2015,"",12,"7.3","Animation, Comedy","Mathieu Auvray","On a desolate island, a sheep with nothing to live for meets a salesman offering him a lifetime of lives.",{}},
 {"f12","Plan 9 from Outer Space",1957,"NR",79,"3.9","Science Fiction, Horror","Ed Wood","Aliens raise the dead to stop humanity from building a doomsday weapon.",{}},
};
// Public-domain episodes; `art` names the publicity photograph used for the show.
struct Episode{const char* id,*series,*name;int season,episode,minutes;double watched;const char* art;};
static const std::vector<Episode> episodes={
 {"e1","Bonanza","A Rose for Lotta",1,1,52,0.4,"e1"},{"e2","The Beverly Hillbillies","The Clampetts Strike Oil",1,1,26,0.7,"e2"},{"e3","The Lucy Show","Lucy Waits Up for Chris",1,1,25,0.25,"e3"},
 // Next Up: other shows than Continue Watching's, so no show is on Home twice.
 {"e4","The Dick Van Dyke Show","My Blonde-Haired Brunette",1,2,26,0,"e7"},{"e5","Gilligan's Island","Home Sweet Hut",1,2,25,0,"e8"},{"e6","The Andy Griffith Show","The Manhunt",1,2,26,0,"e9"},
};
// Classical works (public-domain compositions), with the composers' portraits as covers.
struct Album{const char* id,*name,*artist;int year;};
static const std::vector<Album> albums={{"a1","Suite bergamasque","Claude Debussy",1905},{"a2","Symphony No. 5","Ludwig van Beethoven",1808},{"a3","Nocturnes","Fr\xC3\xA9" "d\xC3\xA9ric Chopin",1833},
 {"a4","The Four Seasons","Antonio Vivaldi",1725},{"a5","Goldberg Variations","Johann Sebastian Bach",1741},{"a6","Eine kleine Nachtmusik","Wolfgang Amadeus Mozart",1787},{"a7","Gymnop\xC3\xA9" "dies","Erik Satie",1888}};
// The freely licensed images tools/fetch_free_art.py saved (build/readme_art/<key>.jpg or .png),
// injected over the generated artwork. Returns whether this key had one.
static bool free_art(const std::string& key,const std::string& as=""){
 std::string file;for(char c:key)file+=isalnum((unsigned char)c)||c=='_'||c=='.'||c=='-'?c:'_';
 for(const char* ext:{".jpg",".png"}){int w,h,n;unsigned char* p=stbi_load(("build/readme_art/"+file+ext).c_str(),&w,&h,&n,4);
  if(p){art_inject(as.empty()?key:as,w,h,std::vector<uint8_t>(p,p+(size_t)w*h*4));stbi_image_free(p);return true;}}
 return false;
}

static Item make(const char* id,const char* name,const char* type,int year){Item i;snprintf(i.base.id,sizeof i.base.id,"%s",id);snprintf(i.base.name,sizeof i.base.name,"%s",name);snprintf(i.base.type,sizeof i.base.type,"%s",type);i.year=year;return i;}
static Item film_item(const Film& f){Item i=make(f.id,f.name,"Movie",f.year);i.rating=f.rating;i.duration=(uint64_t)f.minutes*60000;i.community=f.score;i.genres=f.genres;i.director=f.director;i.overview=f.overview;i.backdrop=true;
 for(auto& c:f.cast){Person p;p.id=std::string("p-")+c.first;p.name=c.first;p.role=c.second;p.image=true;i.people.push_back(p);}return i;}
static Item episode_item(const Episode& e){Item i=make(e.id,e.name,"Episode",2022);i.series=e.series;i.season=e.season;i.episode=e.episode;i.duration=(uint64_t)e.minutes*60000;i.resume=(uint64_t)(i.duration*e.watched);return i;}

// ---- Live TV from the M3U ------------------------------------------------------------
static std::vector<JFChannel> channels_from(const char* path){
 std::vector<JFChannel> out;std::ifstream in(path);std::string line,group;
 static const char* shows[][3]={{"Kids","Cartoon Classics","Adventure Time Marathon"},{"News","World Headlines","Evening Report"},{"Movies","Saturday Night Movie","Classic Cinema"},{"Sports","Match Highlights","Live: Premier Football"},{"Music","Top 40 Countdown","Live Sessions"},{"Documentary","Planet Wild","How It's Made"},{"Entertainment","Late Night Live","Comedy Hour"},{"General","Morning Show","Local News at Six"}};
 const uint64_t now=jf_now_ticks();
 while(std::getline(in,line)&&out.size()<36){
  if(line.rfind("#EXTINF",0)!=0)continue;const auto g=line.find("group-title=\"");group=g==std::string::npos?"":line.substr(g+13,line.find('"',g+13)-g-13);
  const auto comma=line.rfind(',');std::string name=comma==std::string::npos?"Channel":line.substr(comma+1);while(!name.empty()&&(name.back()=='\r'||name.back()==' '))name.pop_back();
  const auto paren=name.rfind(" (");if(paren!=std::string::npos&&name.back()==')')name.resize(paren);   // "(720p)"
  // A varied page: one channel per brand (the first word), not twenty ABC stations.
  static std::vector<std::string> brands;const std::string brand=name.substr(0,name.find(' '));
  if(std::find(brands.begin(),brands.end(),brand)!=brands.end())continue;brands.push_back(brand);
  JFChannel c;memset(&c,0,sizeof c);snprintf(c.id,sizeof c.id,"ch%zu",out.size());snprintf(c.name,sizeof c.name,"%s",name.c_str());snprintf(c.number,sizeof c.number,"%zu",101+out.size());c.favourite=out.size()%7==2;
  const char* title="Live";for(auto& s:shows)if(group==s[0])title=s[1+out.size()%2];if(!strcmp(title,"Live"))title=shows[7][1+out.size()%2];
  snprintf(c.now_title,sizeof c.now_title,"%s",title);c.now_start_ticks=now-(out.size()%5+1)*6*600000000ULL;c.now_end_ticks=now+(out.size()%4+2)*6*600000000ULL;out.push_back(c);}
 return out;
}

int main(int argc,char** argv){
 render_init();theme_apply_builtin(0);assert(storage_init(110));mkdir("build/readme",0755);
 snprintf(g_userid,sizeof g_userid,"%s","readme");snprintf(g_username,sizeof g_username,"%s","alex");snprintf(g_server,sizeof g_server,"%s","http://192.168.1.100:8096");
 puts("Generating artwork...");
 for(auto& f:films){poster(f.id,f.name,std::to_string(f.year));backdrop(std::string(f.id)+":backdrop",f.name);backdrop(std::string(f.id)+":wide",f.name);for(auto& c:f.cast)portrait(std::string("person:p-")+c.first,c.first);}
 for(auto& e:episodes){scene(480,270,seed_of(e.series));char code[24];snprintf(code,sizeof code,"S%d:E%d",e.season,e.episode);art_inject(e.id,480,270,grab(480,270));}
 for(auto& a:albums)album(a.id,a.name,a.artist);
 for(auto lib:{"Movies","Shows","Music","Live TV"}){scene(480,270,seed_of(lib));title_text(480,270,lib,"",100);art_inject(std::string("lib-")+lib,480,270,grab(480,270));}
 portrait("user:readme","alex");
 // The freely licensed images, where there is one: posters, stills (also the
 // wide Continue Watching cards), cast portraits, TV photographs and covers.
 int found=0;
 for(auto& f:films){found+=free_art(f.id);if(free_art(std::string(f.id)+":backdrop")){free_art(std::string(f.id)+":backdrop",std::string(f.id)+":wide");++found;}for(auto& c:f.cast)found+=free_art(std::string("person:p-")+c.first);}
 for(auto& e:episodes)found+=free_art(e.art,e.id);
 for(auto& a:albums)found+=free_art(a.id);
 printf("  %d freely licensed images\n",found);
 puts("Rendering screens...");

 // Home: My Media, Continue Watching, Next Up, Latest Movies, Latest Music.
 HomeView home;home.loaded=true;
 {HomeRow r;r.title="My Media";r.shape=SHAPE_LIBRARY;for(auto lib:{"Movies","Shows","Music","Live TV"}){Item i=make((std::string("lib-")+lib).c_str(),lib,"CollectionFolder",0);r.page.items.push_back(i);}home.rows.push_back(r);}
 {HomeRow r;r.title="Continue Watching";r.shape=SHAPE_WIDE;for(int k:{0,1,2})r.page.items.push_back(episode_item(episodes[k]));
  for(int k:{0,6,4}){Item i=film_item(films[k]);i.resume=i.duration/3;r.page.items.push_back(i);}home.rows.push_back(r);}
 {HomeRow r;r.title="Next Up";r.shape=SHAPE_WIDE;for(int k:{3,4,5})r.page.items.push_back(episode_item(episodes[k]));home.rows.push_back(r);}
 // Latest Movies leaves out the films already in Continue Watching (0, 6 and 4).
 {HomeRow r;r.title="Latest Movies";r.shape=SHAPE_POSTER;for(size_t k=0;k<films.size();++k)if(k!=0&&k!=6&&k!=4)r.page.items.push_back(film_item(films[k]));home.rows.push_back(r);}
 {HomeRow r;r.title="Latest Music";r.shape=SHAPE_SQUARE;for(auto& a:albums){Item i=make(a.id,a.name,"MusicAlbum",a.year);i.artist=a.artist;r.page.items.push_back(i);}home.rows.push_back(r);}
 home.focus=0;home.rows[0].col=0;home.rows[1].col=1;home_step(home,true);draw_home(home,-1);capture("home");
 home.focus=1;home_step(home,true);draw_home(home,-1);capture("home-continue");
 home.focus=3;home.rows[3].col=2;home_step(home,true);draw_home(home,-1);capture("home-latest");

 // Movies library with the A-Z rail, and the rail open.
 // Each film once: the library is exactly the films here.
 Page movies;movies.total=(int)films.size();for(auto& f:films)movies.items.push_back(film_item(f));
 JumpBar jump;jump.show=true;draw_library(movies,"Movies",0,4,false,-1,0,"",&jump);capture("library");
 jump.active=true;jump.sel=9;draw_library(movies,"Movies",0,4,false,-1,0,"",&jump);capture("jumpbar");

 // Details: Charade, part watched, with the cast.
 Item film=film_item(films[0]);film.resume=film.duration*2/5;
 DetailView view;view.version="1080p H264";view.video="1080p \xC2\xB7 H264 \xC2\xB7 SDR";view.audio="English - AAC - Mono - Default";view.audio_chip="AAC Mono";view.subtitles="Off";
 draw_detail(film,view);capture("detail");
 view.row=2;view.col=3;view.cast_first=0;draw_detail(film,view);capture("detail-cast");

 // Quick peek over the library.
 draw_library(movies,"Movies",0,4,false,-1,0,"",&jump);
 {jump.active=false;draw_library(movies,"Movies",0,6,false,-1,0,"",&jump);std::vector<uint32_t> under(pixels);Item peek=film_item(films[6]);peek.cast="Brigitte Helm, Gustav Fr\xC3\xB6hlich, Alfred Abel, Rudolf Klein-Rogge";
  draw_quick_peek(peek,{0,0,0,0},1.0f,"",under.data());capture("peek");}

 // The player: a Big Buck Bunny frame as the picture, the bar, and the L1 picture mode.
 {Item f=film_item(films[1]);PlayerOsd osd;osd.title=f.base.name;osd.subtitle=std::to_string(f.year);osd.audio="AAC Stereo";osd.duration=f.duration;osd.position=f.duration*37/100;osd.started=true;
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);draw_player_osd(osd);capture("player");
  // The seek preview: a closer look at the same still stands in for the frame further on.
  {draw_art_cover(std::string(f.base.id)+":backdrop",-260,-140,900,506);const auto rgba=grab(224,126);std::vector<uint32_t> thumb(224*126);
   for(size_t i=0;i<thumb.size();++i)thumb[i]=0xff000000u|((uint32_t)rgba[i*4]<<16)|((uint32_t)rgba[i*4+1]<<8)|rgba[i*4+2];trickplay_preview(thumb,224,126);}
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);osd.scrubbing=true;osd.target=f.duration*62/100;osd.paused=true;draw_player_osd(osd);capture("player-scrub");
  // R3: the playback statistics over the picture, as for a film played directly.
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);osd.scrubbing=false;osd.paused=false;
  draw_stats_panel({{"Playing","Direct play"},{"Source","Jellyfin server"},{"Limits","20 Mbps,  1080p"},{"Container","MATROSKA,WEBM"},
   {"Video","",true},{"Codec","H264 (High)"},{"Resolution","1920 x 1080  yuv420p"},{"Frame rate","24 in the file,  24.0 decoded"},{"On screen","24.0 fps"},{"Dropped","0 by the decoder,  0 late"},{"Bitrate","8.4 Mbps"},{"Decoder","FFmpeg (software)"},
   {"Audio","",true},{"Format","AAC  Stereo  48.0 kHz"},{"Bitrate","192 kbps"},{"To the PS4","PCM  Stereo  48.0 kHz"},{"Subtitles","eng  SRT"},
   {"Stream","",true},{"Buffer","38.2 s ahead"},{"Network","42.6 Mbps"},{"A/V sync","+3 ms"},{"Memory","2614 MB free"}});capture("player-stats");
  // Options while watching, a film with chapters; the chapter list it opens.
  osd.chapters={{0,"Opening",""},{95000,"The Meadow",""},{190000,"Three Bullies",""},{290000,"The Plan",""},{380000,"Traps",""},{470000,"Payback",""},{560000,"Credits",""}};
  osd.item_id=f.base.id;
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);draw_player_osd(osd);
  osd.options={{"Chapters","7 chapters"},{"Audio / Subtitles","AAC Stereo"},{"Picture","Fit"},{"Playback Statistics","Off"}};osd.options_sel=0;
  draw_options_menu(osd);capture("check-options");
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);draw_player_osd(osd);osd.chapter_sel=3;osd.chapter_now=2;draw_chapter_menu(osd);capture("check-chapters");
  // A show's Options.
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);
  osd.options={{"Next Episode","S1:E3 - The Clampetts Get Culture"},{"Previous Episode",""},{"Audio / Subtitles","AAC Stereo  /  English"},{"Picture","Fit"},{"Skip Intros","Button"},{"Skip Credits","Auto"},{"Playback Statistics","On"}};osd.options_sel=4;
  draw_options_menu(osd);capture("check-options-show");
  // Scrubbing with chapters: their marks on the bar, the chapter named by the time.
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);osd.scrubbing=true;osd.target=300000;draw_player_osd(osd);capture("check-scrub-chapters");
  osd.chapters.clear();osd.scrubbing=false;
  draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);draw_picture_banner("Picture size   Width 100%   Height 100%   Fill",false);capture("picture");}

 // Buffering: Sintel's still under the ring.
 {Item f=film_item(films[2]);buffering_begin(f,"Sintel  \xC2\xB7  2010");buffering_step("BUFFERING",true,"Cancel");buffering_progress(0.62f);for(int t=0;t<700;t+=16){buffering_frame();usleep(16000);}capture("buffering");buffering_finish(false);}

 // Music: Now Playing with each visualizer, on a synthetic mix.
 {PlayerOsd m;m.title="Clair de lune";m.subtitle="Claude Debussy";m.album="Suite bergamasque  \xC2\xB7  1905";m.art="a1";m.audio_only=true;m.started=true;m.duration=330000;m.position=121000;m.track=3;m.tracks=4;m.up_next="Passepied";
  music_viz_reset();std::vector<float> lr(1024*2),wave(688);float bands[MUSIC_VIZ_BANDS];int n=0;
  auto feed=[&](int frames){for(int k=0;k<frames;++k){for(int i=0;i<1024;++i,++n){float t=n/48000.0f;float v=0.45f*sinf(6.2831853f*62*t)*(0.6f+0.4f*sinf(t*3))+0.25f*sinf(6.2831853f*(330+80*sinf(t))*t)+0.12f*sinf(6.2831853f*1760*t)+0.06f*sinf(6.2831853f*7000*t);lr[i*2]=lr[i*2+1]=v;}music_viz_push(lr.data(),1024);music_viz_bands(bands);}};
  const char* names[]={"music-bars","music-aurora","music-halo","music-scope"};
  for(int v=VIZ_BARS;v<=VIZ_SCOPE;++v){feed(12);music_viz_wave(wave.data(),688);m.viz=v;m.viz_toast=v==VIZ_HALO;draw_music_screen(m,bands,MUSIC_VIZ_BANDS,wave.data(),688);capture(names[v]);}}

 // Live TV: the playlist's channels, the grid and the guide.
 if(argc>1){auto chans=channels_from(argv[1]);printf("  %zu channels from the playlist\n",chans.size());
  if(!chans.empty()){livetv_preview(chans,4,false);capture("livetv");livetv_preview(chans,2,true);capture("livetv-guide");}}

 // Settings, a category, and About.
 // The six groups as main.cpp lists them (SETTING_SECTIONS, SETTING_ROWS).
 {const char* dot="  \xC2\xB7  ";
  std::vector<SettingsCategoryView> cats={
   {"Playback",std::string("Quality")+dot+"1080p Playback (Alpha)"+dot+"Resume playback"+dot+"Skip Intros"+dot+"Skip Credits"+dot+"Playback method"+dot+"Playback Statistics",SICON_PLAYBACK},
   {"Subtitles",std::string("Draw Subtitles")+dot+"Subtitle Font"+dot+"Subtitle Colour",SICON_PLAYBACK},
   {"Live TV",std::string("On Now on Home")+dot+"Update Guide"+dot+"Auto-Update Guide"+dot+"Guide Address",SICON_PLAYBACK},
   {"Display",std::string("Theme")+dot+"Language"+dot+"Home Order"+dot+"Screensaver"+dot+"Screensaver Delay",SICON_DISPLAY},
   {"Account",std::string("Share Sign-in")+dot+"Log Out",SICON_ACCOUNT},
   {"System",std::string("Downloads")+dot+"Save Downloads To"+dot+"Software Update"+dot+"About"+dot+"Debug Logging"+dot+"Exit application",SICON_SYSTEM}};
  std::string version=JF_BUILD_VERSION;if(version.size()>1&&version[0]=='0')version.erase(0,1);   // as main.cpp shows it
  draw_settings_categories(cats,0,"Account: alex  \xC2\xB7  http://192.168.1.100:8096","JellyFin [PS] "+version+" \xC2\xB7 built " __DATE__);capture("settings");
  std::vector<SettingsRowView> rows(7);
  rows[0]={"Quality","20 Mbps","The highest bitrate the server streams at. Lower it if playback\nstops to buffer; raise it for a sharper picture on a fast network."};
  rows[1]={"1080p Playback (Alpha)","On","x",true};rows[2]={"Resume playback","On","x",true};rows[3]={"Skip Intros","Button","x"};rows[4]={"Skip Credits","Button","x"};rows[5]={"Playback method","Auto","x"};rows[6]={"Playback Statistics","On","x",true};
  draw_settings_rows(cats[0],rows,0);capture("settings-playback");
  draw_about(0,0);capture("about");}

 // The screensaver over Sintel's still.
 {Item f=film_item(films[2]);screensaver_set_items({{f,"Latest Movies"}});draw_art_cover(std::string(f.base.id)+":backdrop",0,0,WIDTH,HEIGHT);
  for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH*2/3;++x){const int k=(int)(0.86f*255*(1-(float)x/(WIDTH*2/3)));uint32_t& d=pixels[y*WIDTH+x];d=0xff000000|lerp(d&0xffffff,0,k/255.0f);}
  screensaver_preview(0);capture("screensaver");}

 // The boot animation: the mark over black.
 boot_begin();for(int i=0;i<110;++i)boot_pump();capture("boot");
 puts("PASS README screenshots rendered");
}
