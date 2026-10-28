#include "buffering.h"
#include "mono_clock.h"
#include "platform.h"
#include "art.h"
#include "ui_view.h"
#include "boot_anim.h"
#include "language.h"
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <vector>
#include "../vendor/ps3/source/ui/render/buffer_anim.h"

// Jellyfin's brand colours: the ring is drawn in them whatever the artwork.
constexpr uint32_t JF_PURPLE=0xAA5CC3,JF_BLUE=0x00A4DC,VEIL=0x030408;
// PS3 geometry at 720p: the ring's centre, radius and stroke, the mark's box.
constexpr int RING_X=640,RING_Y=318,MARK_BOX=76;
constexpr float RING_R=64,RING_T=3;

static buf_anim anim;
static bool on=false;
static std::string title,label,hint;
static bool show_pct=false;
// The artwork and veil change only when the image arrives: composed once.
static std::vector<uint32_t> backdrop;
static std::string art_key,wide_key,poster_key;
static bool art_drawn=false;

static uint64_t now_us(){return mono_us();}
static uint32_t mix(uint32_t a,uint32_t b,float t){t=std::max(0.0f,std::min(1.0f,t));auto c=[&](int s){return (uint32_t)((((a>>s)&255)*(1-t)+((b>>s)&255)*t)+0.5f)<<s;};return c(16)|c(8)|c(0);}
static void blend(uint32_t& d,uint32_t c,float a){const uint32_t k=(uint32_t)(std::max(0.0f,std::min(1.0f,a))*256),keep=256-k;d=0xff000000|((((d&0xff00ff)*keep+(c&0xff00ff)*k)>>8)&0xff00ff)|((((d&0x00ff00)*keep+(c&0x00ff00)*k)>>8)&0x00ff00);}
// A soft elliptical glow, (1 - d)^2 from the centre out to the radii.
static void glow(int cx,int cy,int rx,int ry,uint32_t color,float alpha){
 if(alpha<=0||rx<1||ry<1)return;auto px=framebuffer();
 for(int y=std::max(0,cy-ry);y<std::min(HEIGHT,cy+ry+1);++y){const float dy=(float)(y-cy)/ry;
  for(int x=std::max(0,cx-rx);x<std::min(WIDTH,cx+rx+1);++x){const float dx=(float)(x-cx)/rx,d2=dx*dx+dy*dy;if(d2>=1)continue;const float f=1-sqrtf(d2);blend(px[y*WIDTH+x],color,f*f*alpha);}}
}
// A ring's arc: `start` and `len` in turns (0 = top, clockwise), the colour and
// opacity running from (c0, a0) at the tail to (c1, a1) at the head. `loop`
// colours the closed ring c0 -> c1 -> c0 round its halves, so it has no seam.
static void ring_arc(float r,float t,float start,float len,uint32_t c0,float a0,uint32_t c1,float a1,bool loop=false){
 if(len<=0||(a0<=0&&a1<=0))return;auto px=framebuffer();const int reach=(int)(r+t)+2;
 const float inner=std::max(0.0f,r-t/2-1),outer=r+t/2+1;
 for(int y=-reach;y<=reach;++y)for(int x=-reach;x<=reach;++x){
  const float fx=x+0.5f,fy=y+0.5f,d2=fx*fx+fy*fy;if(d2<inner*inner||d2>outer*outer)continue;
  const float cover=std::min(1.0f,t/2+0.5f-fabsf(sqrtf(d2)-r));if(cover<=0)continue;
  float turn=atan2f(fx,-fy)/6.2831853f,u=turn-start;u-=floorf(u);if(u>len)continue;
  float k=u/len;if(loop)k=k<0.5f?k*2:2-k*2;
  blend(px[(RING_Y+y)*WIDTH+RING_X+x],mix(c0,c1,k),cover*(a0+(a1-a0)*(loop?1:k)));}
}
// The PS3 eyebrow: small capitals, letter-spaced.
static void eyebrow(int cy,const std::string& s,uint32_t color){
 constexpr int size=13,track=3;int w=0;for(char c:s)w+=face_text_width(std::string(1,c),size,TextFace::Classic)+track;w-=track;
 int x=(WIDTH-w)/2;for(char c:s){std::string one(1,c);face_text(x,cy,one,color,size,TextFace::Classic);x+=face_text_width(one,size,TextFace::Classic)+track;}
}
static std::string pick_art(){
 if(art_state(wide_key)==1)return wide_key;
 if(!poster_key.empty()&&art_state(poster_key)==1)return poster_key;
 return {};
}
static void compose(){
 clear(VEIL);art_drawn=!art_key.empty()&&draw_art_cover(art_key,0,0,WIDTH,HEIGHT,false);
 if(art_drawn){
  // The veil: darkest at the bottom, where the title sits.
  static const float stops[4]={0.0f,0.40f,0.70f,1.0f},base[4]={0.80f,0.70f,0.78f,0.94f};
  for(int y=0;y<HEIGHT;++y){const float v=(float)y/(HEIGHT-1);int s=0;while(s<2&&v>stops[s+1])++s;const float k=(v-stops[s])/(stops[s+1]-stops[s]);blend_rect(0,y,WIDTH,1,VEIL,(int)(255*(base[s]+(base[s+1]-base[s])*k)));}
 }
 else glow(640,330,620,420,g_theme.accent_deep,0.9f);
 backdrop.assign(framebuffer(),framebuffer()+WIDTH*HEIGHT);
}

void buffering_begin(const Item& item,const std::string& name){
 buf_anim_start(&anim,now_us());on=true;title=name;label=T("PREPARING");show_pct=false;hint.clear();
 wide_key=std::string(item.base.id)+":backdrop";
 // A channel's logo is no picture to fill the screen with.
 poster_key=strcmp(item.base.type,"TvChannel")?item.base.id:"";
 art_key=pick_art();compose();
}
void buffering_step(const char* l,bool pct,const char* circle_hint){label=l?l:"";show_pct=pct;hint=circle_hint?circle_hint:"";}
void buffering_progress(float p){buf_anim_progress(&anim,p);}
bool buffering_active(){return on;}

void buffering_frame(){
 if(!on)return;
 // The artwork may still be on its way from the server.
 if(!art_drawn){auto key=pick_art();if(!key.empty()){art_key=key;compose();}}
 buf_frame f;buf_anim_eval(&anim,now_us(),&f);const float ua=f.ui_a;
 memcpy(framebuffer(),backdrop.data(),backdrop.size()*4);
 // The fade: at ui_a = 0 the screen is the veil's black.
 if(ua<0.999f)blend_rect(0,0,WIDTH,HEIGHT,VEIL,(int)(255*(1-ua)));
 glow(RING_X,RING_Y,150,150,g_theme.accent,f.glow_a*ua);
 if(f.track_a>0)ring_arc(RING_R,RING_T*0.66f,0,1,0xFFFFFF,f.track_a*0.6f*ua,0xFFFFFF,f.track_a*0.6f*ua);
 const float arc=std::min(1.0f,f.arc),head=f.ring_a*ua;
 if(arc>=0.999f)ring_arc(RING_R,RING_T,f.ring_angle,1,JF_PURPLE,head,JF_BLUE,head,true);
 else ring_arc(RING_R,RING_T,f.ring_angle-arc,arc,JF_PURPLE,0,JF_BLUE,head);
 draw_jf_mark(RING_X,RING_Y,MARK_BOX*f.mark_scale,f.mark_a*ua);
 // Text, faded with everything else.
 if(!label.empty())eyebrow(400,label,mix(VEIL,g_theme.text_dim,ua));
 if(show_pct){const std::string pct=std::to_string(f.pct)+"%";face_text((WIDTH-face_text_width(pct,20,TextFace::Classic))/2,424,pct,mix(VEIL,g_theme.text,ua),20,TextFace::Classic);}
 if(!title.empty())face_text(57,606,fit_face(title,1166,22,TextFace::Classic),mix(VEIL,g_theme.text,ua*0.92f),22,TextFace::Classic);
 if(!hint.empty()&&ua>0.5f&&anim.phase==BUF_LOADING)draw_hints(57,680,{{'C',hint.c_str()}});
}
void buffering_overlay(uint64_t since_us,const char* l){
 const float t=(now_us()-since_us)/1e6f,in=std::min(1.0f,t/0.35f),ease=in*in*(3-2*in);
 // A veil over the picture, then the buffering screen's ring as a comet: it
 // turns once a second and its tail breathes between a quarter and a half.
 blend_rect(0,0,WIDTH,HEIGHT,VEIL,(int)(150*ease));
 const float breath=0.5f-0.5f*cosf(t*6.2831853f/2.4f);
 glow(RING_X,RING_Y,150,150,g_theme.accent,(0.18f+0.14f*breath)*ease);
 ring_arc(RING_R,RING_T*0.66f,0,1,0xFFFFFF,0.12f*ease,0xFFFFFF,0.12f*ease);
 const float len=0.25f+0.25f*breath,head=t*1.0f;
 ring_arc(RING_R,RING_T,head-len,len,JF_PURPLE,0,JF_BLUE,ease);
 draw_jf_mark(RING_X,RING_Y,MARK_BOX*(0.96f+0.04f*breath),ease);
 if(l&&*l)eyebrow(400,l,mix(VEIL,g_theme.text_dim,ease));
}
void buffering_run(const std::function<void()>& work){
 if(!on){work();return;}
 struct Job{const std::function<void()>* work;std::atomic<bool> done{false};}job;job.work=&work;
 pthread_t thread;
 if(pthread_create(&thread,nullptr,[](void* p)->void*{auto j=(Job*)p;(*j->work)();j->done=true;return nullptr;},&job)){work();return;}
 // present() waits for the display, so this loop runs at its rate.
 while(!job.done){buffering_frame();present();}
 pthread_join(thread,nullptr);
}
void buffering_finish(bool ready){
 if(!on)return;const uint64_t now=now_us();if(ready)buf_anim_ready(&anim,now);else buf_anim_cancel(&anim,now);
 // Bounded by buffer_anim.h: the minimum showing plus about half a second.
 while(!buf_anim_done(&anim,now_us())){buffering_frame();present();}
 clear(0);present();on=false;backdrop.clear();backdrop.shrink_to_fit();
}
