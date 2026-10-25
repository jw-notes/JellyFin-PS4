#include "art.h"
#include "app.h"
#include "platform.h"
#include <map>
#include <vector>
#include <algorithm>
#include <deque>
#include <set>
#include <pthread.h>
#include <math.h>
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
// A decoded image, plus the sizes it has been drawn at: scaling happens once
// per size, after which drawing is a row copy (opaque) or a straight blend.
struct Scaled{int w=0,h=0,dx=0,dy=0;bool cover=false,circle=false,opaque=true;std::vector<uint32_t> px;unsigned long long used=0;};
struct Art{std::vector<uint8_t> pixels;int width=0,height=0;unsigned long long used=0;mutable std::vector<Scaled> sized;};
static std::map<std::string,Art> cache;
static unsigned long long counter=0;
static Art mark;
static bool mark_loaded=false;
struct ArtJob{std::string id,url,token;};
static pthread_mutex_t art_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t art_changed=PTHREAD_COND_INITIALIZER;
static std::deque<ArtJob> art_queue;
static std::set<std::string> pending;
static pthread_t art_thread;
static bool art_running=false,art_stopping=false,art_fetching=false;
static void* artwork_worker(void*){
 for(;;){
  pthread_mutex_lock(&art_lock);
  while(art_queue.empty()&&!art_stopping)pthread_cond_wait(&art_changed,&art_lock);
  if(art_stopping){pthread_mutex_unlock(&art_lock);return nullptr;}
  ArtJob job=art_queue.front();art_queue.pop_front();art_fetching=true;pthread_mutex_unlock(&art_lock);
  Art art;std::vector<uint8_t> buffer(2*1024*1024);int size=http_fetch_binary(job.url.c_str(),job.token.c_str(),buffer.data(),(int)buffer.size());
  if(size>0){int channels;auto data=stbi_load_from_memory(buffer.data(),size,&art.width,&art.height,&channels,4);if(data){art.pixels.assign(data,data+art.width*art.height*4);stbi_image_free(data);}}
  pthread_mutex_lock(&art_lock);
  if(!art_stopping){if(cache.size()>=64){auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});cache.erase(oldest);}art.used=++counter;cache[job.id]=std::move(art);}
  pending.erase(job.id);art_fetching=false;pthread_cond_broadcast(&art_changed);pthread_mutex_unlock(&art_lock);
 }
}
void load_image(const std::string& key,const std::string& path){
 if(key.empty()||key.rfind("nav:",0)==0)return;
 pthread_mutex_lock(&art_lock);
 auto existing=cache.find(key);if(existing!=cache.end()){existing->second.used=++counter;pthread_mutex_unlock(&art_lock);return;}
 if(pending.count(key)){pthread_mutex_unlock(&art_lock);return;}
 if(!art_running){art_stopping=false;if(pthread_create(&art_thread,nullptr,artwork_worker,nullptr)){pthread_mutex_unlock(&art_lock);plog("Artwork worker could not start");return;}art_running=true;}
 while(art_queue.size()>=32){pending.erase(art_queue.front().id);art_queue.pop_front();}
 pending.insert(key);art_queue.push_back({key,std::string(g_server)+path,g_token});pthread_cond_signal(&art_changed);pthread_mutex_unlock(&art_lock);
}
void load_art(const std::string& id){if(id.rfind("nav:",0)==0)return;load_image(id,"/Items/"+encode(id)+"/Images/Primary?MaxWidth=320&Quality=85");}
int art_state(const std::string& key){pthread_mutex_lock(&art_lock);auto i=cache.find(key);int state=i==cache.end()?0:i->second.pixels.empty()?-1:1;pthread_mutex_unlock(&art_lock);return state;}
void art_wait(){pthread_mutex_lock(&art_lock);while((art_fetching||!art_queue.empty())&&!art_stopping)pthread_cond_wait(&art_changed,&art_lock);pthread_mutex_unlock(&art_lock);}
void art_stop(bool clear_cache){
 pthread_mutex_lock(&art_lock);bool join=art_running;art_stopping=true;art_queue.clear();pending.clear();pthread_cond_broadcast(&art_changed);pthread_mutex_unlock(&art_lock);
 if(join)pthread_join(art_thread,nullptr);
 pthread_mutex_lock(&art_lock);art_running=false;art_stopping=false;art_fetching=false;if(clear_cache)cache.clear();pthread_mutex_unlock(&art_lock);
}

static void scale_into(const Art& a,int w,int h,bool cover,bool circle,Scaled& out){
 int dw=w,dh=h;float scale=cover?std::max((float)w/a.width,(float)h/a.height):std::min((float)w/a.width,(float)h/a.height);
 if(!cover){dw=std::max(1,(int)(a.width*scale));dh=std::max(1,(int)(a.height*scale));}
 const int source_w=cover?(int)(w/scale):a.width,source_h=cover?(int)(h/scale):a.height,sx0=(a.width-source_w)/2,sy0=(a.height-source_h)/2;
 out.w=dw;out.h=dh;out.dx=cover?0:(w-dw)/2;out.dy=cover?0:(h-dh)/2;out.cover=cover;out.circle=circle;out.opaque=true;out.px.resize((size_t)dw*dh);
 // Box-average each target pixel's footprint when shrinking: no shimmer on small cards.
 for(int row=0;row<dh;++row){const int y0=sy0+row*source_h/dh,y1=std::max(y0+1,std::min(a.height,sy0+(row+1)*source_h/dh));
  for(int col=0;col<dw;++col){
   if(circle){int ddx=2*col-dw+1,ddy=2*row-dh+1;if(ddx*ddx+ddy*ddy>dw*dw){out.px[(size_t)row*dw+col]=0;out.opaque=false;continue;}}
   const int x0=sx0+col*source_w/dw,x1=std::max(x0+1,std::min(a.width,sx0+(col+1)*source_w/dw));
   unsigned r=0,g=0,b=0,al=0,k=0;const int stepx=std::max(1,(x1-x0)/3),stepy=std::max(1,(y1-y0)/3);
   for(int sy=y0;sy<y1;sy+=stepy)for(int sx=x0;sx<x1;sx+=stepx){auto p=&a.pixels[((size_t)std::min(a.height-1,sy)*a.width+std::min(a.width-1,sx))*4];r+=p[0];g+=p[1];b+=p[2];al+=p[3];++k;}
   r/=k;g/=k;b/=k;al/=k;if(al<255)out.opaque=false;out.px[(size_t)row*dw+col]=(al<<24)|(r<<16)|(g<<8)|b;}}
}
static bool draw_image(const Art& a,int x,int y,int w,int h,bool cover,bool circle=false,bool keep=true){
 if(a.pixels.empty()||w<=0||h<=0)return false;
 Scaled* s=nullptr;Scaled temp;
 for(auto& c:a.sized)if(c.cover==cover&&c.circle==circle&&((cover&&c.w==w&&c.h==h)||(!cover&&c.w+2*c.dx==w&&c.h+2*c.dy==h)))s=&c;
 if(!s){
  if(keep){if(a.sized.size()>=3)a.sized.erase(std::min_element(a.sized.begin(),a.sized.end(),[](const Scaled& p,const Scaled& q){return p.used<q.used;}));a.sized.emplace_back();s=&a.sized.back();}
  else s=&temp;
  scale_into(a,w,h,cover,circle,*s);
 }
 s->used=++counter;x+=s->dx;y+=s->dy;
 const int c0=std::max(0,-x),c1=std::min(s->w,WIDTH-x);if(c0>=c1)return true;auto fb=framebuffer();
 for(int row=std::max(0,-y);row<s->h&&y+row<HEIGHT;++row){
  const uint32_t* src=&s->px[(size_t)row*s->w];uint32_t* dst=fb+(size_t)(y+row)*WIDTH+x;
  if(s->opaque){for(int c=c0;c<c1;++c)dst[c]=0xff000000|src[c];continue;}
  for(int c=c0;c<c1;++c){const uint32_t p=src[c],al=p>>24;if(!al)continue;if(al==255){dst[c]=0xff000000|p;continue;}const uint32_t a1=al+1,k=256-a1,d=dst[c];
   dst[c]=0xff000000|(((((d&0xff00ff)*k)+((p&0xff00ff)*a1))>>8)&0xff00ff)|(((((d&0x00ff00)*k)+((p&0x00ff00)*a1))>>8)&0x00ff00);}
 }
 return true;
}
bool draw_art(const std::string& id,int x,int y,int w,int h,bool keep){pthread_mutex_lock(&art_lock);auto i=cache.find(id);bool result=i!=cache.end()&&draw_image(i->second,x,y,w,h,false,false,keep);pthread_mutex_unlock(&art_lock);return result;}
bool draw_art_cover(const std::string& id,int x,int y,int w,int h,bool keep){pthread_mutex_lock(&art_lock);auto i=cache.find(id);bool result=i!=cache.end()&&draw_image(i->second,x,y,w,h,true,false,keep);pthread_mutex_unlock(&art_lock);return result;}
bool draw_art_circle(const std::string& id,int x,int y,int d){pthread_mutex_lock(&art_lock);auto i=cache.find(id);bool result=i!=cache.end()&&draw_image(i->second,x,y,d,d,true,true);pthread_mutex_unlock(&art_lock);return result;}
void draw_brand_mark(int x,int y,int size){
 if(!mark_loaded){mark_loaded=true;int channels;auto p=stbi_load("/app0/assets/jellyfin-mark.png",&mark.width,&mark.height,&channels,4);if(!p)p=stbi_load("assets/jellyfin-mark.png",&mark.width,&mark.height,&channels,4);if(p){mark.pixels.assign(p,p+mark.width*mark.height*4);stbi_image_free(p);}}
 draw_image(mark,x,y,size,size,false);
}

// The PS4 Options button has no image in the PS3 prompt sheet: draw its
// three-line glyph in a rounded badge, sized like the other prompts.
static int options_icon(int x,int y,int height){
 int w=height*3/2,r=height/3;uint32_t edge=g_theme.text_dim;
 for(int row=0;row<height;++row){int inset=0;if(row<r)inset=r-(int)sqrtf((float)(r*r-(r-row)*(r-row)));else if(row>=height-r){int d=row-(height-r-1);inset=r-(int)sqrtf((float)std::max(0,r*r-d*d));}
  bool border=row==0||row==height-1;if(border)rect(x+inset,y+row,w-2*inset,1,edge);else{rect(x+inset,y+row,1,1,edge);rect(x+w-inset-1,y+row,1,1,edge);}}
 int lw=w/2,lx=x+(w-lw)/2;for(int k=0;k<3;++k)rect(lx,y+height*(3+k*2)/10+1,lw,std::max(1,height/12),edge);
 return w;
}
// The touchpad: a wide rounded pad with its click line.
static int touchpad_icon(int x,int y,int height){
 int w=height*2,r=height/4;uint32_t edge=g_theme.text_dim;
 for(int row=0;row<height;++row){int inset=0;if(row<r)inset=r-(int)sqrtf((float)(r*r-(r-row)*(r-row)));else if(row>=height-r){int d=row-(height-r-1);inset=r-(int)sqrtf((float)std::max(0,r*r-d*d));}
  if(row==0||row==height-1)rect(x+inset,y+row,w-2*inset,1,edge);else{rect(x+inset,y+row,1,1,edge);rect(x+w-inset-1,y+row,1,1,edge);}}
 rect(x+w/2,y+2,1,height-4,edge);return w;
}
int draw_button_icon(char glyph,int x,int y,int height){
 if(glyph=='O')return options_icon(x,y,height);
 if(glyph=='P')return touchpad_icon(x,y,height);
 static std::map<char,Art> prompts;
 const char* name=glyph=='X'?"cross":glyph=='C'?"circle":glyph=='S'?"square":glyph=='T'?"triangle":glyph=='D'?"dpad":glyph=='l'?"l1":glyph=='r'?"r1":glyph=='L'?"l2":glyph=='R'?"r2":nullptr;
 if(!name)return 0;
 auto found=prompts.find(glyph);
 if(found==prompts.end()){Art art;int channels;std::string path=std::string("/app0/assets/prompts/")+name+".png";auto data=stbi_load(path.c_str(),&art.width,&art.height,&channels,4);if(!data){path=std::string("assets/prompts/")+name+".png";data=stbi_load(path.c_str(),&art.width,&art.height,&channels,4);}if(data){art.pixels.assign(data,data+art.width*art.height*4);stbi_image_free(data);}found=prompts.emplace(glyph,std::move(art)).first;}
 const auto& art=found->second;if(art.pixels.empty())return 0;int width=art.width*height/art.height;draw_image(art,x,y,width,height,false);return width;
}
int draw_hints(int x,int y,std::initializer_list<Hint> hints,int size){
 for(const auto& hint:hints){int icon=draw_button_icon(hint.glyph,x,y+(size>=20?0:1),size+2);if(icon)x+=icon+(hint.label&&*hint.label?7:3);
  if(hint.label&&*hint.label){face_text(x,y+(size>=20?1:2),hint.label,g_theme.text_dim,size-2,TextFace::Classic);x+=face_text_width(hint.label,size-2,TextFace::Classic)+22;}}
 return x;
}
int hints_width(std::initializer_list<Hint> hints,int size){
 int x=0;for(const auto& hint:hints){x+=(hint.glyph=='O'?(size+2)*3/2:hint.glyph=='P'?(size+2)*2:size+2)+(hint.label&&*hint.label?7:3);if(hint.label&&*hint.label)x+=face_text_width(hint.label,size-2,TextFace::Classic)+22;}return x;
}
#ifdef HOST_TEST
// Host previews and benchmarks: put a decoded image in the cache.
void art_inject(const std::string& key,int w,int h,const std::vector<uint8_t>& rgba){pthread_mutex_lock(&art_lock);Art a;a.width=w;a.height=h;a.pixels=rgba;a.used=++counter;cache[key]=std::move(a);pthread_mutex_unlock(&art_lock);}
#endif
