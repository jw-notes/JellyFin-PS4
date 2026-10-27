#include "boot_anim.h"
#include "thread_priority.h"
#include "mono_clock.h"
#include "app.h"
#include "platform.h"
#include "language.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <vector>
#include "stb_image.h"
#include "../vendor/ps3/source/ui/render/boot_seq.h"
#include "../vendor/ps3/source/gfx/jfmark_hd_png.h"

// The mark raster: 88 viewBox units across, of which the bell is 72.
constexpr float MARK_SPAN_U=88,MARK_BELL_U=72;
// The centred mark, as fractions of the screen height (PS3 boot_anim.cpp).
constexpr float CENTRE_BELL_H=0.17f,CENTRE_Y_H=0.47f,HALO_RADIUS=1.55f,STATUS_GAP_H=0.045f;
constexpr float SPARK_APEX_Y=-0.47f,SPARK_SIZE=0.38f;
// Where the header draws the brand mark (ui_view.cpp header_chrome).
constexpr float LOCKUP_X=48,LOCKUP_Y=12,LOCKUP_BOX=36;

static bool on=false;
static BootSeq seq;
static BootFrame frame;
static BootHeaderPose pose;
static bool pose_live=false;
static uint64_t last_us=0,began_us=0;
// Frame pacing, for the log: frames slower than a 60 Hz refresh, and the slowest.
static unsigned slow_frames=0,slow_handoff=0;static uint64_t slowest_us=0,slowest_handoff_us=0;
static float mark_cx,mark_cy,mark_bell;
// Premultiplied RGBA levels of the 256 px mark, each half the last, so the
// bilinear sample never minifies by more than 2:1.
struct Level{int px;std::vector<uint32_t> rgba;};
static std::vector<Level> levels;

static uint64_t now_us(){return mono_us();}
static bool gate_on(){FILE* f=fopen(jf_data_path("bootanim.txt"),"r");if(!f)return true;int v=1;if(fscanf(f,"%d",&v)!=1)v=1;fclose(f);return v!=0;}
static bool load_mark(){
 int w,h,comp;unsigned char* img=stbi_load_from_memory(jfmark_hd_cool_png,(int)jfmark_hd_cool_png_len,&w,&h,&comp,4);
 if(!img)return false;if(w!=JFMARK_HD_MASTER||h!=JFMARK_HD_MASTER){stbi_image_free(img);return false;}
 Level base{w,std::vector<uint32_t>((size_t)w*h)};
 for(int i=0;i<w*h;++i){uint32_t a=img[i*4+3];base.rgba[i]=(a<<24)|((img[i*4]*a/255)<<16)|((img[i*4+1]*a/255)<<8)|(img[i*4+2]*a/255);}
 stbi_image_free(img);levels.clear();levels.push_back(std::move(base));
 while(levels.back().px>32){const Level& big=levels.back();Level small{big.px/2,std::vector<uint32_t>((size_t)(big.px/2)*(big.px/2))};
  for(int y=0;y<small.px;++y)for(int x=0;x<small.px;++x){uint32_t sum[4]={};for(int k=0;k<4;++k){uint32_t p=big.rgba[(y*2+k/2)*big.px+x*2+k%2];for(int c=0;c<4;++c)sum[c]+=(p>>(c*8))&255;}small.rgba[y*small.px+x]=((sum[3]/4)<<24)|((sum[2]/4)<<16)|((sum[1]/4)<<8)|(sum[0]/4);}
  levels.push_back(std::move(small));}
 return true;
}
// Two packed premultiplied pixels mixed, t of 256 toward b: two multiplies for all four channels.
static inline uint32_t mix_px(uint32_t a,uint32_t b,uint32_t t){
 const uint32_t s=256-t;
 return ((((a&0x00ff00ff)*s+(b&0x00ff00ff)*t)>>8)&0x00ff00ff)|((((a>>8)&0x00ff00ff)*s+((b>>8)&0x00ff00ff)*t)&0xff00ff00);
}
// The mark centred at (cx,cy) with bell height `bell`, its glint band added
// where the mark covers, composited at `opacity`. Size and position stay
// fractional, so a mark that grows, breathes or glides moves by fractions of a
// pixel instead of stepping. Integer bilinear sampling in 16.16 fixed point:
// the float version cost most of a 16 ms frame on the console.
static void draw_mark(float cx,float cy,float bell,float opacity,float glint_pos,float glint){
 if(levels.empty()||bell<=0||opacity<=0)return;
 const float box=bell*MARK_SPAN_U/MARK_BELL_U;if(box<1)return;
 const Level* l=&levels.back();for(const auto& level:levels)if(level.px>=box)l=&level;
 const float fx0=cx-box*0.5f,fy0=cy-box*0.5f,scale=l->px/box;auto pixels=framebuffer();const int n=l->px;const uint32_t* src=l->rgba.data();
 const uint32_t op=(uint32_t)(std::min(1.0f,opacity)*256+0.5f);const int32_t step=(int32_t)(scale*65536);
 const int x_first=std::max(0,(int)floorf(fx0)),x_last=std::min(WIDTH,(int)ceilf(fx0+box));
 for(int py=std::max(0,(int)floorf(fy0));py<std::min(HEIGHT,(int)ceilf(fy0+box));++py){
  const float v=(py+0.5f-fy0)/box;if(v<0||v>1)continue;
  const float fy=v*n-0.5f;const int y0=(int)floorf(fy);const uint32_t ty=(uint32_t)((fy-y0)*256);
  const uint32_t* r0=src+std::max(0,std::min(n-1,y0))*n;const uint32_t* r1=src+std::max(0,std::min(n-1,y0+1))*n;
  int32_t fx=(int32_t)(((x_first+0.5f-fx0)*scale-0.5f)*65536);uint32_t* row=pixels+py*WIDTH;
  for(int px=x_first;px<x_last;++px,fx+=step){
   const int ix=fx>>16,x0=std::max(0,std::min(n-1,ix)),x1=std::max(0,std::min(n-1,ix+1));const uint32_t tx=(uint32_t)(fx>>8)&255;
   const uint32_t p00=r0[x0],p01=r0[x1],p10=r1[x0],p11=r1[x1];
   if(!((p00|p01|p10|p11)>>24))continue;   // all four taps transparent
   uint32_t s=mix_px(mix_px(p00,p01,tx),mix_px(p10,p11,tx),ty);
   if(glint>0){const uint32_t sa=s>>24;if(sa){const float u=(px+0.5f-fx0)/box;const uint32_t add=(uint32_t)(boot_glint_at(u,v,glint_pos)*glint*BOOT_GLINT_GAIN*sa);
     auto up=[&](int sh){const uint32_t c=std::min(sa,((s>>sh)&255)+add);s=(s&~(255u<<sh))|(c<<sh);};up(16);up(8);up(0);}}
   if(op<256)s=mix_px(0,s,op);
   const uint32_t sa=s>>24;if(!sa)continue;const uint32_t keep=256-sa,d=row[px];
   row[px]=0xff000000|((((d&0x00ff00ff)*keep>>8)&0x00ff00ff)+(s&0x00ff00ff))|((((d&0x0000ff00)*keep>>8)&0x0000ff00)+(s&0x0000ff00));}}
}
// A soft radial glow, strongest at the centre: (1 - d/r)^2, looked up by the
// squared distance so no pixel needs a square root.
static void glow(float cx,float cy,float radius,uint32_t color,float alpha){
 if(alpha<=0||radius<1)return;static int table[257];static bool ready=false;
 if(!ready){for(int i=0;i<=256;++i){float d=sqrtf(i/256.0f);table[i]=(int)((1-d)*(1-d)*256);}ready=true;}
 const int r=(int)radius,r2=r*r,a=(int)(alpha*256);auto pixels=framebuffer();const uint32_t rb=color&0xff00ff,g=color&0x00ff00;
 // The table index by multiplying with a reciprocal: a division per pixel was
 // a few milliseconds a frame on the console.
 const uint32_t inv=(256u<<16)/(uint32_t)std::max(1,r2);
 for(int y=-r;y<=r;++y){int py=(int)cy+y;if(py<0||py>=HEIGHT)continue;const int half=(int)sqrtf((float)(r2-y*y));const uint32_t yy=(uint32_t)(y*y);
  for(int x=-half;x<=half;++x){int px=(int)cx+x;if(px<0||px>=WIDTH)continue;const uint32_t k=(uint32_t)(table[std::min(256u,(((uint32_t)(x*x)+yy)*inv)>>16)]*a>>8);if(!k)continue;
   uint32_t& o=pixels[py*WIDTH+px];const uint32_t keep=256-k;o=0xff000000|((((o&0xff00ff)*keep+rb*k)>>8)&0xff00ff)|((((o&0x00ff00)*keep+g*k)>>8)&0x00ff00);}}
}
// A four-armed star, arms tapering to points, rotated by `rot`.
static void star(float cx,float cy,float size,float width,float rot,float alpha){
 if(alpha<=0||size<1)return;const int r=(int)size+1;const float c=cosf(rot),s=sinf(rot);auto pixels=framebuffer();
 for(int y=-r;y<=r;++y){int py=(int)cy+y;if(py<0||py>=HEIGHT)continue;for(int x=-r;x<=r;++x){int px=(int)cx+x;if(px<0||px>=WIDTH)continue;
  float u=fabsf(x*c+y*s),v=fabsf(-x*s+y*c);float cover=0;
  if(u<=size){float edge=width*(1-u/size)-v;cover=std::max(cover,std::min(1.0f,edge+0.5f));}
  if(v<=size){float edge=width*(1-v/size)-u;cover=std::max(cover,std::min(1.0f,edge+0.5f));}
  if(cover<=0)continue;float k=cover*alpha;uint32_t& o=pixels[py*WIDTH+px];int dr=(o>>16)&255,dg=(o>>8)&255,db=o&255;
  o=0xff000000|((dr+(int)((255-dr)*k))<<16)|((dg+(int)((248-dg)*k))<<8)|(db+(int)((255-db)*k));}}
}
static void sparkle(float x,float y,float bell,float spark,float rot){
 if(spark<=0||bell<=0)return;const float size=bell*SPARK_SIZE*(0.55f+0.45f*spark);
 glow(x,y,size*0.45f,0xFFF8FF,spark*0.40f);star(x,y,size,size*0.11f,rot,spark);star(x,y,size*0.5f,size*0.08f,rot+0.78539816f,spark*0.7f);
}
static void status_line(float cx,float mark_bottom,float alpha){
 if(alpha<=0)return;const char* label=T("Connecting to server");const int size=17;
 uint32_t c=g_theme.text_dim;int k=(int)(alpha*255);uint32_t color=((((c>>16)&255)*k/255)<<16)|((((c>>8)&255)*k/255)<<8)|((c&255)*k/255);
 face_text((int)(cx-face_text_width(label,size,TextFace::Classic)/2.0f),(int)(mark_bottom+STATUS_GAP_H*HEIGHT),label,color,size,TextFace::Classic);
}
static void layout(){
 const float bell0=CENTRE_BELL_H*HEIGHT,cx0=0.5f*WIDTH,cy0=CENTRE_Y_H*HEIGHT;pose_live=false;
 if(frame.done)return;
 if(!frame.xmb_visible){mark_cx=cx0;mark_cy=cy0;mark_bell=bell0*frame.mark_scale;return;}
 const float bell1=LOCKUP_BOX*MARK_BELL_U/MARK_SPAN_U;
 mark_bell=boot_dock_size_px(bell0,bell1,frame.dock_size);
 boot_dock_point(cx0,cy0,LOCKUP_X+LOCKUP_BOX/2,LOCKUP_Y+LOCKUP_BOX/2,frame.dock_pos,&mark_cx,&mark_cy);
 pose_live=true;pose.show_mark=frame.dock_pos>=1&&frame.dock_size>=1;pose.slide_px=BOOT_WORD_SLIDE_EM*20;
 for(int i=0;i<8;++i){pose.word_reveal[i]=frame.word_reveal[i];pose.word_alpha[i]=frame.word_alpha[i];}
}
// Drawing time alone (not waiting for the display), for the mark over black
// and for the handoff: a slow frame with a short draw was held up by other
// threads; a long draw is the drawing itself.
static uint64_t draw_from=0,draw_max_us[2]={},draw_total_us[2]={};static unsigned draw_count[2]={};
static void draw_begin(){draw_from=now_us();}
static void draw_end(){if(!draw_from)return;const uint64_t d=now_us()-draw_from;draw_from=0;const int k=frame.xmb_visible?1:0;draw_max_us[k]=std::max(draw_max_us[k],d);draw_total_us[k]+=d;++draw_count[k];}
static void advance(){
 uint64_t now=now_us();float dt=(float)(now-last_us)/1000.0f;if(now-last_us>20000)++slow_frames;slowest_us=std::max(slowest_us,now-last_us);
 // The same for the handoff alone (Home rising, the mark docking), the part that shows a stutter most.
 if(frame.xmb_visible){if(now-last_us>20000)++slow_handoff;slowest_handoff_us=std::max(slowest_handoff_us,now-last_us);}
 last_us=now;
 boot_seq_step(&seq,dt);boot_seq_frame(&seq,&frame);layout();
 // The mark's levels stay: draw_jf_mark() uses them after the boot.
 if(on&&frame.done){on=false;pose_live=false;char b[320];snprintf(b,sizeof b,"boot: done after %u frames, %.0f ms (%llu ms real), %u slow, slowest %llu ms; handoff %u slow, slowest %llu ms; draw: mark %.1f ms average / %.1f max, handoff %.1f / %.1f%s",seq.frames,(double)seq.t,(unsigned long long)((now-began_us)/1000),slow_frames,(unsigned long long)(slowest_us/1000),slow_handoff,(unsigned long long)(slowest_handoff_us/1000),
  draw_count[0]?draw_total_us[0]/1000.0/draw_count[0]:0.0,draw_max_us[0]/1000.0,draw_count[1]?draw_total_us[1]/1000.0/draw_count[1]:0.0,draw_max_us[1]/1000.0,seq.left?" (dismissed)":"");plog(b);}
}
static void draw_over(bool pre){
 const float box=mark_bell*MARK_SPAN_U/MARK_BELL_U;
 // Before Home shows, mark and halo fade in and out together over black.
 const float fade=pre?frame.mark_opacity:1.0f;
 glow(mark_cx,mark_cy,mark_bell*HALO_RADIUS,g_theme.lk_mark_a,frame.halo*BOOT_HALO_PEAK*fade);
 if(pre||!pose.show_mark)draw_mark(mark_cx,mark_cy,mark_bell,fade,frame.glint_pos,frame.glint);
 status_line(mark_cx,mark_cy+0.5f*box,frame.status);
 sparkle(mark_cx,mark_cy+SPARK_APEX_Y*mark_bell,mark_bell,frame.spark,frame.spark_rot);
}

void boot_begin(){
 if(on)return;if(!gate_on()){plog("boot: animation off (bootanim.txt = 0)");return;}
 clear(0);present();
 if(levels.empty()&&!load_mark()){plog("boot: mark decode failed");return;}
 boot_seq_begin(&seq);boot_seq_frame(&seq,&frame);last_us=began_us=now_us();slow_frames=slow_handoff=0;slowest_us=slowest_handoff_us=0;for(int k=0;k<2;++k){draw_max_us[k]=draw_total_us[k]=0;draw_count[k]=0;}on=true;plog("boot: begin");
}
bool boot_active(){return on;}
void boot_pump(){if(!on)return;advance();if(!on)return;draw_begin();clear(0);if(!frame.xmb_visible)draw_over(true);draw_end();present();}
void boot_run(const std::function<void()>& work){
 if(!on){work();return;}
 struct Job{const std::function<void()>* work;std::atomic<bool> done{false};}job;job.work=&work;
 pthread_t thread;
 if(pthread_create(&thread,nullptr,[](void* p)->void*{background_priority();auto j=(Job*)p;(*j->work)();j->done=true;return nullptr;},&job)){work();return;}
 while(!job.done){boot_pump();if(!on)usleep(16000);}
 pthread_join(thread,nullptr);
}
void boot_leave(){if(!on)return;boot_seq_signal(&seq,BOOT_SIG_LEAVE);while(on){boot_pump();}clear(0);present();}
void boot_ready(){if(on)boot_seq_signal(&seq,BOOT_SIG_XMB);}
void boot_tick(){if(on){advance();if(on)draw_begin();}}
bool boot_covers_home(){return on&&!frame.xmb_visible;}
void boot_overlay(){
 if(!on)return;
 // Until the handoff starts the screen is the animation's alone: black, the
 // mark and its halo, with nothing of Home showing through.
 if(!frame.xmb_visible){clear(0);draw_over(true);draw_end();return;}
 // The veil lifts from the bottom edge up, with a soft band.
 float ys[BOOT_VEIL_ROWS_MAX];unsigned char as[BOOT_VEIL_ROWS_MAX];const int n=boot_veil_rows(frame.veil,(float)HEIGHT,ys,as);
 for(int i=0;i+1<n;++i){int y0=(int)ys[i],y1=(int)ys[i+1];if(as[i]==255&&as[i+1]==255){rect(0,y0,WIDTH,y1-y0,0);continue;}for(int y=y0;y<y1;++y){float t=y1>y0?(float)(y-y0)/(y1-y0):0;blend_rect(0,y,WIDTH,1,0,(int)(as[i]+(as[i+1]-as[i])*t));}}
 draw_over(false);draw_end();
}
void draw_jf_mark(float cx,float cy,float box,float opacity){if(levels.empty()&&!load_mark())return;draw_mark(cx,cy,box*MARK_BELL_U/MARK_SPAN_U,opacity,0,0);}
void boot_skip(){if(on)boot_seq_signal(&seq,BOOT_SIG_SKIP);}
const BootHeaderPose* boot_header_pose(){return on&&pose_live?&pose:nullptr;}
