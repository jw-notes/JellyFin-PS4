// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// See lockup.h. Ported from JellyFin [PS] for PS3 (vendor/ps3/source/ui/render/
// ui_widgets.cpp, ui_text.cpp), on the PS4's software renderer.
#include "lockup.h"
#include "boot_anim.h"
#include "art.h"
#include "platform.h"
#include "app.h"
#include "stb_image.h"
#include <math.h>
#include <string>
#include <algorithm>
#include "../vendor/ps3/source/gfx/jfmark_png.h"

// ---- the mark -----------------------------------------------------------------------
MarkVariant mark_variant(){
 const uint32_t a=g_theme.lk_mark_a&0xFFFFFF,b=g_theme.lk_mark_b&0xFFFFFF;
 if(a==0xAA5CC3&&b==0x00A4DC)return MARK_COOL;
 if(a==0xE8B45C)return MARK_GOLD;
 return MARK_TINT;
}
uint64_t mark_theme_key(){return ((uint64_t)(g_theme.lk_mark_a&0xFFFFFF)<<32)|(g_theme.lk_mark_b&0xFFFFFF);}

// Each coloured pixel is placed along the cool raster's own purple -> blue
// gradient and moved to the same place along the theme's pair, at the same
// brightness; the PlayStation symbol, the white edge and the shadow (no colour
// of their own) are left as they are. (PS3 xmb_mark_tint.)
void mark_tint(unsigned char* rgba,int n){
 const uint32_t a=g_theme.lk_mark_a,b=g_theme.lk_mark_b;
 const float P[3]={170,92,195},B[3]={0,164,220};
 const float A2[3]={(float)((a>>16)&0xFF),(float)((a>>8)&0xFF),(float)(a&0xFF)};
 const float B2[3]={(float)((b>>16)&0xFF),(float)((b>>8)&0xFF),(float)(b&0xFF)};
 const float d[3]={B[0]-P[0],B[1]-P[1],B[2]-P[2]};const float dd=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
 for(int i=0;i<n;++i){
  unsigned char* px=rgba+(size_t)i*4;
  const int mx=std::max(px[0],std::max(px[1],px[2])),mn=std::min(px[0],std::min(px[1],px[2]));
  if(mx-mn<36)continue;   // grey, white, black: not the gradient
  float t=((px[0]-P[0])*d[0]+(px[1]-P[1])*d[1]+(px[2]-P[2])*d[2])/dd;t=std::max(0.0f,std::min(1.0f,t));
  const float lum_src=0.299f*px[0]+0.587f*px[1]+0.114f*px[2];
  const float base[3]={P[0]+d[0]*t,P[1]+d[1]*t,P[2]+d[2]*t};const float lum_base=0.299f*base[0]+0.587f*base[1]+0.114f*base[2];
  float c[3];for(int k=0;k<3;++k)c[k]=A2[k]+(B2[k]-A2[k])*t;
  const float lum_c=0.299f*c[0]+0.587f*c[1]+0.114f*c[2];
  const float k=lum_base>1.0f?lum_src/lum_base:1.0f,scale=lum_c>1.0f?k:1.0f;
  for(int j=0;j<3;++j){const float v=c[j]*scale;px[j]=(unsigned char)(v<0?0:v>255?255:(int)(v+0.5f));}
 }
}
bool mark_rgba(std::vector<uint8_t>& rgba,int& w,int& h){
 const MarkVariant v=mark_variant();int comp=0;
 unsigned char* img=v==MARK_GOLD?stbi_load_from_memory(jfmark_gold_png,(int)jfmark_gold_png_len,&w,&h,&comp,4):stbi_load_from_memory(jfmark_cool_png,(int)jfmark_cool_png_len,&w,&h,&comp,4);
 if(!img)return false;
 if(v==MARK_TINT)mark_tint(img,w*h);
 rgba.assign(img,img+(size_t)w*h*4);stbi_image_free(img);return true;
}

// ---- the wordmark ---------------------------------------------------------------------
static uint32_t ramp_at(const uint32_t* stop,int n,float t){
 if(n==1||t<=0)return stop[0]&0xFFFFFF;if(t>=1)return stop[n-1]&0xFFFFFF;
 const float f=t*(n-1);const int i=(int)f;const float g=f-i;const uint32_t a=stop[i],b=stop[i+1];
 auto ch=[&](int sh){return (uint32_t)(((a>>sh)&0xFF)*(1-g)+((b>>sh)&0xFF)*g);};
 return (ch(16)<<16)|(ch(8)<<8)|ch(0);
}
// The ramp darkened, as the extrusion under the face (PS3 lk_ramp_shade).
static void shade(const uint32_t* in,uint32_t* out,float k){
 for(int i=0;i<4;++i){const uint32_t c=in[i];out[i]=((uint32_t)(((c>>16)&0xFF)*k)<<16)|((uint32_t)(((c>>8)&0xFF)*k)<<8)|(uint32_t)((c&0xFF)*k);}
}
// Toward the header's background, for a letter fading in (alpha 0..1).
static uint32_t fade(uint32_t c,float a){
 if(a>=1)return c&0xFFFFFF;const uint32_t bg=g_theme.bg_top;
 auto ch=[&](int sh){const int cb=(bg>>sh)&0xFF,cc=(c>>sh)&0xFF;return (uint32_t)(cb+(int)((cc-cb)*a));};
 return (ch(16)<<16)|(ch(8)<<8)|ch(0);
}
static const char WORD[]="JELLYFIN";
static constexpr int LETTERS=8;
// One pass of the word at (x,y): each letter in the ramp's colour at its centre
// (null: flat black), slid and faded by the pose if there is one.
static void word_pass(float x,int y,const uint32_t* ramp,const BootHeaderPose* pose){
 const float track=LOCKUP_WORD_PX*0.02f;
 float adv[LETTERS],total=0;for(int i=0;i<LETTERS;++i){adv[i]=(float)face_text_width(std::string(1,WORD[i]),LOCKUP_WORD_PX,TextFace::Lockup);total+=adv[i]+(i?track:0);}
 float pen=x;
 for(int i=0;i<LETTERS;++i){
  if(i)pen+=track;
  const uint32_t colour=ramp?ramp_at(ramp,4,(pen-x+adv[i]*0.5f)/total):0;
  float gx=pen,alpha=1;
  if(pose){alpha=std::max(0.0f,std::min(1.0f,pose->word_alpha[i]));
   // Slides into place from the left, never from further left than the word's start.
   float d=std::min(pen-x,pose->slide_px);if(pose->word_reveal[i]<1)gx=pen-d*(1-pose->word_reveal[i]);}
  if(alpha>0)face_text((int)lroundf(gx),y,std::string(1,WORD[i]),fade(colour,alpha),LOCKUP_WORD_PX,TextFace::Lockup);
  pen+=adv[i];
 }
}
void draw_lockup(const BootHeaderPose* pose){
 // The mark: its raster's box is larger than the bell (it carries the shadow).
 if(!pose||pose->show_mark){
  const int box=(int)lroundf(LOCKUP_BELL*MARK_SPAN_U/MARK_BELL_U),off=(int)lroundf(LOCKUP_BELL*MARK_PAD_U/MARK_BELL_U);
  draw_brand_mark(LOCKUP_BELL_X-off,LOCKUP_CY-LOCKUP_BELL/2-off,box);
 }
 // The word, its ink centred on the mark's line. Back to front: the drop
 // shadow, two extruded layers stepping down-right (deepest first), the face.
 const uint32_t ramp[4]={g_theme.lk_word_a,g_theme.lk_word_b,g_theme.lk_word_c,g_theme.lk_word_d};
 uint32_t deep[4],mid[4];shade(ramp,deep,0.28f);shade(ramp,mid,0.50f);
 const float x=(float)(LOCKUP_BELL_X+LOCKUP_BELL+LOCKUP_GAP);
 const int y=LOCKUP_CY-face_ink_middle(WORD,LOCKUP_WORD_PX,TextFace::Lockup);
 constexpr int st=1;
 word_pass(x+st,y+3*st,nullptr,pose);
 word_pass(x+st,y+2*st,deep,pose);
 word_pass(x,y+st,mid,pose);
 word_pass(x,y,ramp,pose);
}
