#include "screensaver.h"
#include "platform.h"
#include "art.h"
#include <algorithm>
#include <string.h>
#include <time.h>
#include <unistd.h>

// PS3 timing (experience.h / ui_ambient.cpp).
constexpr uint64_t IN_MS=700,LIFT_MS=1600,OUT1_MS=300,OUT2_MS=400,HOLD_MS=14000,FADE_MS=1600,PREFETCH_MS=5000;
constexpr float COVER_FULL=0.94f;
static const int delays[]={30,60,120,180,300,600,900,1800};   // seconds

static std::vector<SaverItem> slides;
static uint64_t last_input=0;
static int inhibit=0;
static bool prefetched=false;

static uint64_t clock_ms(){timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000+ts.tv_nsec/1000000;}
static float smooth(float t){t=std::max(0.0f,std::min(1.0f,t));return t*t*(3-2*t);}
static uint32_t scale_rgb(uint32_t c,float k){int a=(int)(k*256);return (((c>>16)&255)*a>>8)<<16|(((c>>8)&255)*a>>8)<<8|((c&255)*a>>8);}
// An episode shows its series' backdrop, as Home's wide cards do on the PS3.
static std::string image_id(const Item& item){return !strcmp(item.base.type,"Episode")&&!item.series_id.empty()?item.series_id:item.base.id;}
static std::string key_of(const SaverItem& s){return "saver:"+image_id(s.item);}
static void request(int index){if(index>=0&&index<(int)slides.size())load_image(key_of(slides[index]),"/Items/"+encode(image_id(slides[index].item))+"/Images/Backdrop?MaxWidth=1280&Quality=80");}

ScreensaverInhibit::ScreensaverInhibit(){++inhibit;}
ScreensaverInhibit::~ScreensaverInhibit(){--inhibit;last_input=clock_ms();}

void screensaver_set_items(const std::vector<SaverItem>& items){
 slides.clear();
 for(const auto& s:items){if(slides.size()>=12)break;bool dup=false;for(const auto& o:slides)if(image_id(o.item)==image_id(s.item))dup=true;
  // Only items that can have a backdrop: their own, or their series'.
  if(!dup&&(s.item.backdrop||!strcmp(s.item.base.type,"Episode")))slides.push_back(s);}
 prefetched=false;
}
std::string screensaver_delay_label(){int s=settings.saver_delay;return s<60?std::to_string(s)+" sec":std::to_string(s/60)+" min";}
void screensaver_delay_step(int dir){
 const int n=(int)(sizeof delays/sizeof *delays);int i=0;while(i<n-1&&delays[i]<settings.saver_delay)++i;
 settings.saver_delay=delays[(i+(dir<0?n-1:1))%n];last_input=clock_ms();
}

// ---- drawing ----------------------------------------------------------------
// A slide composed once: its backdrop filling the screen under the PS3
// scrims (left and bottom for the text, top for the lockup).
struct Composed{int index=-1;std::vector<uint32_t> pixels;};
static bool compose(int index,Composed& out){
 if(!draw_art_cover(key_of(slides[index]),0,0,WIDTH,HEIGHT,false))return false;
 static int column[WIDTH],row[HEIGHT];
 for(int x=0;x<WIDTH;++x)column[x]=x<WIDTH*2/3?(int)(0.86f*255*(1-(float)x/(WIDTH*2/3))):0;
 for(int y=0;y<HEIGHT;++y){int b=y<HEIGHT*2/5?0:(int)(0.92f*255*(y-HEIGHT*2/5)/(float)(HEIGHT-HEIGHT*2/5)),t=y<HEIGHT/6?(int)(0.55f*255*(1-(float)y/(HEIGHT/6))):0;row[y]=255-(255-b)*(255-t)/255;}
 auto p=framebuffer();
 for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x){int a=255-(255-column[x])*(255-row[y])/255,keep=255-a;uint32_t d=p[y*WIDTH+x];p[y*WIDTH+x]=0xff000000|((((d>>16)&255)*keep/255)<<16)|((((d>>8)&255)*keep/255)<<8)|((d&255)*keep/255);}
 out.index=index;out.pixels.assign(p,p+WIDTH*HEIGHT);return true;
}
// out = (a * (1-t) + b * t) * k, k and t in 0..256.
static void present_mix(const std::vector<uint32_t>* a,const std::vector<uint32_t>* b,int t,int k){
 auto p=framebuffer();
 for(int i=0;i<WIDTH*HEIGHT;++i){uint32_t ca=a?(*a)[i]:0,cb=b?(*b)[i]:0;int r=((((ca>>16)&255)*(256-t)+((cb>>16)&255)*t)>>8)*k>>8,g=((((ca>>8)&255)*(256-t)+((cb>>8)&255)*t)>>8)*k>>8,bl=(((ca&255)*(256-t)+(cb&255)*t)>>8)*k>>8;p[i]=0xff000000|(r<<16)|(g<<8)|bl;}
}
static std::string fit(const std::string& v,int w,int size){if(face_text_width(v,size,TextFace::Classic)<=w)return v;std::string s=v;while(!s.empty()&&face_text_width(s+"...",size,TextFace::Classic)>w){size_t e=s.size()-1;while(e&&((unsigned char)s[e]&0xc0)==0x80)--e;s.resize(e);}return s+"...";}
static std::vector<std::string> wrap(const std::string& text,int width,int size,int lines){
 std::vector<std::string> out;size_t at=0;
 while(at<text.size()&&(int)out.size()<lines){while(at<text.size()&&(text[at]==' '||text[at]=='\n'||text[at]=='\r'))++at;size_t end=at,space=at;
  while(end<text.size()&&text[end]!='\n'){size_t next=end+1;while(next<text.size()&&((unsigned char)text[next]&0xc0)==0x80)++next;if(face_text_width(text.substr(at,next-at),size,TextFace::Classic)>width)break;if(text[end]==' ')space=end;end=next;}
  if(end==at)break;if(end<text.size()&&text[end]!='\n'&&space>at)end=space;std::string line=text.substr(at,end-at);at=end;
  if((int)out.size()==lines-1&&at<text.size())line=fit(line+"...",width,size);out.push_back(line);}
 return out;
}
// One slide's text at opacity t: bottom left, laid out upward from y=640.
static void text_block(int index,float t){
 if(index<0||t<0.04f)return;const auto& s=slides[index];const Item& item=s.item;const int x=72,w=640;int y=640;
 auto ink=[&](uint32_t c,float k){return scale_rgb(c,t*k);};
 if(!item.overview.empty()){auto lines=wrap(item.overview,w,17,3);y-=(int)lines.size()*23;for(size_t i=0;i<lines.size();++i)face_text(x,y+(int)i*23,lines[i],ink(g_theme.text,0.88f),17,TextFace::Classic);y-=16;}
 std::vector<std::string> parts;if(item.year)parts.push_back(std::to_string(item.year));if(!item.rating.empty())parts.push_back(item.rating);
 if(item.duration>=60000){unsigned m=(unsigned)(item.duration/60000);parts.push_back(m>=60?std::to_string(m/60)+"h "+std::to_string(m%60)+"m":std::to_string(m)+"m");}
 std::string meta;for(size_t i=0;i<parts.size();++i)meta+=(i?"  \xC2\xB7  ":"")+parts[i];
 if(!meta.empty()){y-=26;face_text(x,y,meta,ink(g_theme.text_dim,1),16,TextFace::Classic);}
 const bool episode=!strcmp(item.base.type,"Episode");
 if(episode){std::string line=item.episode?"S"+std::to_string(item.season)+" E"+std::to_string(item.episode)+"  \xC2\xB7  "+item.base.name:item.base.name;y-=30;face_text(x,y,fit(line,w,20),ink(g_theme.text,0.95f),20,TextFace::Classic);}
 y-=60;face_text(x,y,fit(episode&&!item.series.empty()?item.series:item.base.name,w,46),ink(g_theme.text,1),46,TextFace::Classic);
 std::string eyebrow;for(char c:s.row)eyebrow+=(char)(c>='a'&&c<='z'?c-32:c);
 int ex=x;for(size_t i=0;i<eyebrow.size();++i){std::string ch(1,eyebrow[i]);face_text(ex,y-22,ch,ink(g_theme.accent_alt,0.9f),13,TextFace::Classic);ex+=face_text_width(ch,13,TextFace::Classic)+2;}
}
static void lockup(float t){if(t<0.04f)return;draw_brand_mark(48,12,36);face_text(96,17,"Jellyfin [PS]",scale_rgb(g_theme.text,t),20,TextFace::Classic);}

// ---- the modal loop ---------------------------------------------------------
static void run(){
 plog(("Screensaver: on, "+std::to_string(slides.size())+" items").c_str());
 // The interface as it was, to dissolve away from and back into.
 std::vector<uint32_t> ui(framebuffer(),framebuffer()+WIDTH*HEIGHT);
 Composed slot[2];int cur=-1,nxt=-1;bool first=true;uint64_t cur_t=0,xf_t=0;std::vector<bool> none(slides.size(),false);
 auto slot_of=[&](int index)->Composed*{for(auto& s:slot)if(s.index==index)return &s;return nullptr;};
 auto candidate=[&](int from){for(size_t k=0;k<slides.size();++k){int i=(int)((from+k)%slides.size());if(art_state(key_of(slides[i]))<0)none[i]=true;if(!none[i])return i;}return -1;};
 auto ready=[&](int index){if(slot_of(index))return true;if(art_state(key_of(slides[index]))!=1)return false;Composed* free=&slot[0];for(auto& s:slot)if(s.index!=cur)free=&s;return compose(index,*free);};
 const uint64_t start=clock_ms();uint64_t out_at=0;float art_at_out=0;
 for(;;){
  const uint64_t now=clock_ms(),t=now-start;
  if(!out_at&&buttons()){out_at=now;art_at_out=t<IN_MS?-1:smooth((float)(t-IN_MS-200)/LIFT_MS);}
  // Slides: the first with a backdrop, then each held and crossfaded.
  if(cur<0){int c=candidate(0);if(c>=0){request(c);if(ready(c)){cur=c;cur_t=now;first=true;}}}
  else if(xf_t){if(now-xf_t>=FADE_MS){cur=nxt;nxt=-1;cur_t=now;xf_t=0;first=false;}}
  else{int n=candidate(cur+1);if(n>=0&&n!=cur){if(now-cur_t>=HOLD_MS/2)request(n);if(now-cur_t>=HOLD_MS&&ready(n)){nxt=n;xf_t=now;}}}
  float art=t<IN_MS?0:smooth((float)(t-IN_MS-200)/LIFT_MS);
  if(out_at){const uint64_t o=now-out_at;
   if(art_at_out>=0&&o<OUT1_MS)art=art_at_out*(1-smooth((float)o/OUT1_MS));
   else{const uint64_t o2=art_at_out>=0?o-OUT1_MS:o;if(o2>=OUT2_MS)break;
    // The interface comes back from under the cover.
    const float c0=art_at_out>=0?COVER_FULL:COVER_FULL*smooth((float)(out_at-start)/IN_MS);
    present_mix(&ui,nullptr,0,(int)(256*(1-c0*(1-smooth((float)o2/OUT2_MS)))));present();usleep(1000);continue;}}
  if(t<IN_MS&&!out_at){present_mix(&ui,nullptr,0,(int)(256*(1-COVER_FULL*smooth((float)t/IN_MS))));present();usleep(1000);continue;}
  const float ac=cur<0?0:first?smooth((float)(now-cur_t)/FADE_MS):1,an=xf_t?smooth((float)(now-xf_t)/FADE_MS):0;
  Composed* a=cur>=0?slot_of(cur):nullptr;Composed* b=xf_t?slot_of(nxt):nullptr;
  present_mix(a?&a->pixels:nullptr,b?&b->pixels:nullptr,(int)(256*an),(int)(256*art*(b?1:ac)));
  lockup(art);
  if(xf_t){text_block(cur,art*(1-std::min(1.0f,an*2)));text_block(nxt,art*std::max(0.0f,an*2-1));}else text_block(cur,art*ac);
  present();usleep(1000);
 }
 plog("Screensaver: off");
}
bool screensaver_poll(uint32_t raw,uint64_t now){
 if(raw||inhibit||!settings.saver||!last_input){last_input=raw||!last_input||inhibit?now:last_input;if(raw)prefetched=false;return false;}
 const uint64_t idle=now-last_input,delay=(uint64_t)settings.saver_delay*1000;
 if(slides.empty())return false;
 // Start the first backdrop shortly before, so it opens on artwork.
 if(!prefetched&&idle+PREFETCH_MS>=delay){prefetched=true;request(0);request(1);}
 if(idle<delay)return false;
 run();while(buttons())usleep(16000);
 last_input=clock_ms();prefetched=false;return true;
}
#ifdef HOST_TEST
// Host previews: one slide's text and lockup over whatever is in the frame.
void screensaver_preview(int index){lockup(1);text_block(index,1);}
#endif
