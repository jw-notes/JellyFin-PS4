#include "boot_anim.h"
#include "app.h"
#include "platform.h"
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
static uint64_t last_us=0;
static float mark_cx,mark_cy,mark_bell;
// Premultiplied RGBA levels of the 256 px mark, each half the last, so the
// bilinear sample never minifies by more than 2:1.
struct Level{int px;std::vector<uint32_t> rgba;};
static std::vector<Level> levels;

static uint64_t now_us(){timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000000+ts.tv_nsec/1000;}
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
static uint32_t sample(const Level& l,float u,float v){
 float fx=u*l.px-0.5f,fy=v*l.px-0.5f;int x0=(int)floorf(fx),y0=(int)floorf(fy);float tx=fx-x0,ty=fy-y0;float out[4]={};
 for(int k=0;k<4;++k){int x=std::max(0,std::min(l.px-1,x0+k%2)),y=std::max(0,std::min(l.px-1,y0+k/2));float wgt=(k%2?tx:1-tx)*(k/2?ty:1-ty);uint32_t p=l.rgba[y*l.px+x];for(int c=0;c<4;++c)out[c]+=wgt*((p>>(c*8))&255);}
 return ((uint32_t)(out[3]+0.5f)<<24)|((uint32_t)(out[2]+0.5f)<<16)|((uint32_t)(out[1]+0.5f)<<8)|(uint32_t)(out[0]+0.5f);
}
// The mark centred at (cx,cy) with bell height `bell`, its glint band added
// where the mark covers, composited at `opacity`.
static void draw_mark(float cx,float cy,float bell,float opacity,float glint_pos,float glint){
 if(levels.empty()||bell<=0||opacity<=0)return;
 const int box=(int)(bell*MARK_SPAN_U/MARK_BELL_U+0.5f);if(box<=0)return;
 const Level* l=&levels.back();for(const auto& level:levels)if(level.px>=box)l=&level;
 const int x0=(int)(cx-box*0.5f+0.5f),y0=(int)(cy-box*0.5f+0.5f);auto pixels=framebuffer();
 for(int y=0;y<box;++y){int py=y0+y;if(py<0||py>=HEIGHT)continue;float v=(y+0.5f)/box;
  for(int x=0;x<box;++x){int px=x0+x;if(px<0||px>=WIDTH)continue;float u=(x+0.5f)/box;uint32_t s=sample(*l,u,v);float a=((s>>24)&255)/255.0f;if(a<=0.002f)continue;
   float r=(s>>16)&255,g=(s>>8)&255,b=s&255;
   if(glint>0){float add=boot_glint_at(u,v,glint_pos)*glint*BOOT_GLINT_GAIN*255*a;r=std::min(255.0f*a,r+add);g=std::min(255.0f*a,g+add);b=std::min(255.0f*a,b+add);}
   a*=opacity;r*=opacity;g*=opacity;b*=opacity;uint32_t d=pixels[py*WIDTH+px];float keep=1-a;
   pixels[py*WIDTH+px]=0xff000000|((uint32_t)std::min(255.0f,r+((d>>16)&255)*keep)<<16)|((uint32_t)std::min(255.0f,g+((d>>8)&255)*keep)<<8)|(uint32_t)std::min(255.0f,b+(d&255)*keep);}}
}
// A soft radial glow, strongest at the centre: (1 - d/r)^2, looked up by the
// squared distance so no pixel needs a square root.
static void glow(float cx,float cy,float radius,uint32_t color,float alpha){
 if(alpha<=0||radius<1)return;static int table[257];static bool ready=false;
 if(!ready){for(int i=0;i<=256;++i){float d=sqrtf(i/256.0f);table[i]=(int)((1-d)*(1-d)*256);}ready=true;}
 const int r=(int)radius,r2=r*r,a=(int)(alpha*256);auto pixels=framebuffer();const uint32_t rb=color&0xff00ff,g=color&0x00ff00;
 for(int y=-r;y<=r;++y){int py=(int)cy+y;if(py<0||py>=HEIGHT)continue;const int half=(int)sqrtf((float)(r2-y*y));
  for(int x=-half;x<=half;++x){int px=(int)cx+x;if(px<0||px>=WIDTH)continue;const uint32_t k=(uint32_t)(table[(x*x+y*y)*256/r2]*a>>8);if(!k)continue;
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
 if(alpha<=0)return;const char* label="Connecting to server";const int size=17;
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
static void advance(){
 uint64_t now=now_us();float dt=(float)(now-last_us)/1000.0f;last_us=now;
 boot_seq_step(&seq,dt);boot_seq_frame(&seq,&frame);layout();
 if(on&&frame.done){on=false;pose_live=false;levels.clear();levels.shrink_to_fit();char b[96];snprintf(b,sizeof b,"boot: done after %u frames, %.0f ms%s",seq.frames,(double)seq.t,seq.left?" (dismissed)":"");plog(b);}
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
 if(!load_mark()){plog("boot: mark decode failed");return;}
 boot_seq_begin(&seq);boot_seq_frame(&seq,&frame);last_us=now_us();on=true;plog("boot: begin");
}
bool boot_active(){return on;}
void boot_pump(){if(!on)return;advance();if(!on)return;clear(0);if(!frame.xmb_visible)draw_over(true);present();}
void boot_run(const std::function<void()>& work){
 if(!on){work();return;}
 struct Job{const std::function<void()>* work;std::atomic<bool> done{false};}job;job.work=&work;
 pthread_t thread;
 if(pthread_create(&thread,nullptr,[](void* p)->void*{auto j=(Job*)p;(*j->work)();j->done=true;return nullptr;},&job)){work();return;}
 while(!job.done){boot_pump();if(!on)usleep(16000);}
 pthread_join(thread,nullptr);
}
void boot_leave(){if(!on)return;boot_seq_signal(&seq,BOOT_SIG_LEAVE);while(on){boot_pump();}clear(0);present();}
void boot_ready(){if(on)boot_seq_signal(&seq,BOOT_SIG_XMB);}
void boot_tick(){if(on)advance();}
bool boot_covers_home(){return on&&!frame.xmb_visible;}
void boot_overlay(){
 if(!on)return;
 // Until the handoff starts the screen is the animation's alone: black, the
 // mark and its halo, with nothing of Home showing through.
 if(!frame.xmb_visible){clear(0);draw_over(true);return;}
 // The veil lifts from the bottom edge up, with a soft band.
 float ys[BOOT_VEIL_ROWS_MAX];unsigned char as[BOOT_VEIL_ROWS_MAX];const int n=boot_veil_rows(frame.veil,(float)HEIGHT,ys,as);
 for(int i=0;i+1<n;++i){int y0=(int)ys[i],y1=(int)ys[i+1];if(as[i]==255&&as[i+1]==255){rect(0,y0,WIDTH,y1-y0,0);continue;}for(int y=y0;y<y1;++y){float t=y1>y0?(float)(y-y0)/(y1-y0):0;blend_rect(0,y,WIDTH,1,0,(int)(as[i]+(as[i+1]-as[i])*t));}}
 draw_over(false);
}
void boot_skip(){if(on)boot_seq_signal(&seq,BOOT_SIG_SKIP);}
const BootHeaderPose* boot_header_pose(){return on&&pose_live?&pose:nullptr;}
