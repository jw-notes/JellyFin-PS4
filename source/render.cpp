#include "app.h"
#include "platform.h"
#include <algorithm>
#include <map>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "opensans_regular.h"
#include "satoshi_regular.h"
#include "robotocondensed_bold.h"
#include "notosansjp.h"
static stbtt_fontinfo fonts[3],japanese;
struct Glyph {int w=0,h=0,x=0,y=0,advance=0;unsigned char* data=nullptr;};
static std::map<uint64_t,Glyph> glyphs;
void render_init(){static bool initialized=false;if(initialized)return;initialized=true;stbtt_InitFont(&fonts[0],OpenSans_Regular_ttf,0);stbtt_InitFont(&fonts[1],Satoshi_Regular_otf,0);stbtt_InitFont(&fonts[2],RobotoCondensed_Bold_ttf,0);stbtt_InitFont(&japanese,NotoSansJP_otf,0);}
static unsigned codepoint(const std::string& s,size_t& i){unsigned cp=(unsigned char)s[i];if(cp>=128){unsigned n=(cp&0xe0)==0xc0?1:(cp&0xf0)==0xe0?2:(cp&0xf8)==0xf0?3:0;cp&=(1u<<(6-n))-1;while(n--&&i+1<s.size()){unsigned next=(unsigned char)s[++i];if((next&0xc0)!=0x80)return '?';cp=(cp<<6)|(next&63);}}return cp;}
static Glyph& glyph(unsigned cp,int size,TextFace face){
 size=std::max(8,std::min(64,size));uint64_t key=((uint64_t)face<<56)|((uint64_t)size<<32)|cp;auto found=glyphs.find(key);if(found!=glyphs.end())return found->second;
 if(glyphs.size()>=4096){for(auto& entry:glyphs)stbtt_FreeBitmap(entry.second.data,nullptr);glyphs.clear();}
 auto f=&fonts[(int)face];if(!stbtt_FindGlyphIndex(f,cp))f=&japanese;float scale=stbtt_ScaleForPixelHeight(f,(float)size);Glyph g;int advance;stbtt_GetCodepointHMetrics(f,cp,&advance,nullptr);g.advance=(int)(advance*scale);g.data=stbtt_GetCodepointBitmap(f,0,scale,cp,&g.w,&g.h,&g.x,&g.y);return glyphs.emplace(key,g).first->second;
}
int face_text_width(const std::string& value,int size,TextFace face){int width=0;for(size_t i=0;i<value.size();++i){unsigned cp=codepoint(value,i);if(cp>=32)width+=glyph(cp,size,face).advance;}return width;}
int text_width(const std::string& s,int size){return face_text_width(s,size,TextFace::Default);}
void clear(uint32_t color){std::fill(framebuffer(),framebuffer()+WIDTH*HEIGHT,0xff000000|color);}
void rect(int x,int y,int w,int h,uint32_t color){int x1=std::max(0,x),y1=std::max(0,y),x2=std::min(WIDTH,x+w),y2=std::min(HEIGHT,y+h);if(x1>=x2)return;auto fb=framebuffer();const uint32_t v=0xff000000|color;for(int row=y1;row<y2;++row)std::fill(fb+row*WIDTH+x1,fb+row*WIDTH+x2,v);}
// Packed arithmetic: red and blue share one multiply, green another.
void blend_rect(int x,int y,int w,int h,uint32_t color,int alpha){if(alpha<=0)return;if(alpha>=255){rect(x,y,w,h,color);return;}int x1=std::max(0,x),y1=std::max(0,y),x2=std::min(WIDTH,x+w),y2=std::min(HEIGHT,y+h);if(x1>=x2)return;const uint32_t a=(uint32_t)alpha+1,keep=256-a,rb=(color&0xff00ff)*a,g=(color&0x00ff00)*a;auto fb=framebuffer();for(int row=y1;row<y2;++row){auto line=fb+row*WIDTH;for(int col=x1;col<x2;++col){uint32_t d=line[col];line[col]=0xff000000|((((d&0xff00ff)*keep+rb)>>8)&0xff00ff)|((((d&0x00ff00)*keep+g)>>8)&0x00ff00);}}}
void face_text(int x,int y,const std::string& s,uint32_t color,int size,TextFace face,int maxwidth){auto fb=framebuffer();int left=x,baseline=size==32?27:(size*7+4)/8;for(size_t i=0;i<s.size();++i){unsigned cp=codepoint(s,i);if(cp<32)continue;const auto& g=glyph(cp,size,face);if(x+g.advance>left+maxwidth)break;for(int row=0;row<g.h;++row)for(int col=0;col<g.w;++col){int px=x+g.x+col,py=y+baseline+g.y+row;if(px<0||py<0||px>=WIDTH||py>=HEIGHT)continue;int a=g.data[row*g.w+col];if(!a)continue;auto& dst=fb[py*WIDTH+px];int r=(((color>>16)&255)*a+((dst>>16)&255)*(255-a))/255,green=(((color>>8)&255)*a+((dst>>8)&255)*(255-a))/255,b=((color&255)*a+(dst&255)*(255-a))/255;dst=0xff000000|(r<<16)|(green<<8)|b;}x+=g.advance;}}
void text(int x,int y,const std::string& s,uint32_t color,int size,int maxwidth){face_text(x,y,s,color,size,TextFace::Default,maxwidth);}
void wrapped(int x,int y,const std::string& str,uint32_t color,int lines){size_t at=0;for(int n=0;n<lines&&at<str.size();n++){size_t end=std::min(at+90,str.size());if(end<str.size()){auto sp=str.rfind(' ',end);if(sp>at&&sp!=std::string::npos)end=sp;}text(x,y+30*n,str.substr(at,end-at),color);at=end;while(at<str.size()&&str[at]==' ')at++;}}
