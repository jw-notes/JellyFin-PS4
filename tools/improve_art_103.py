from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/art.h';f.write_text('''#pragma once
#include <string>
void load_art(const std::string& id);
bool draw_art(const std::string& id,int x,int y,int w,int h);
bool draw_art_cover(const std::string& id,int x,int y,int w,int h);
void draw_brand_mark(int x,int y,int size);
''',newline='\n')
f=p/'source/art.cpp';t=f.read_text();a=t.index('struct Art');t=t[:a]+'''struct Art{std::vector<uint8_t> pixels;int width=0,height=0;unsigned long long used=0;};
static std::map<std::string,Art> cache;
static unsigned long long counter=0;
static Art mark;
static bool mark_loaded=false;
void load_art(const std::string& id){
 if(id.empty())return;
 auto existing=cache.find(id);if(existing!=cache.end()){existing->second.used=++counter;return;}
 if(cache.size()>=64){auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});cache.erase(oldest);}
 Art art;art.used=++counter;std::vector<uint8_t> buffer(512*1024);
 std::string url=std::string(g_server)+"/Items/"+encode(id)+"/Images/Primary?MaxWidth=320&Quality=85";
 int size=http_fetch_binary(url.c_str(),g_token,buffer.data(),(int)buffer.size());
 if(size>0){int channels;uint8_t* data=stbi_load_from_memory(buffer.data(),size,&art.width,&art.height,&channels,4);if(data){art.pixels.assign(data,data+art.width*art.height*4);stbi_image_free(data);}}
 cache[id]=std::move(art);
}
static bool draw_image(const Art& a,int x,int y,int w,int h,bool cover){
 if(a.pixels.empty()||w<=0||h<=0)return false;
 int dw=w,dh=h;float scale=cover?std::max((float)w/a.width,(float)h/a.height):std::min((float)w/a.width,(float)h/a.height);
 if(!cover){dw=(int)(a.width*scale);dh=(int)(a.height*scale);x+=(w-dw)/2;y+=(h-dh)/2;}
 int source_w=cover?(int)(w/scale):a.width,source_h=cover?(int)(h/scale):a.height;
 int sx0=(a.width-source_w)/2,sy0=(a.height-source_h)/2;
 for(int row=0;row<dh;row++)for(int col=0;col<dw;col++){
  int px=x+col,py=y+row;if(px<0||py<0||px>=WIDTH||py>=HEIGHT)continue;
  int sx=std::min(a.width-1,sx0+col*source_w/dw),sy=std::min(a.height-1,sy0+row*source_h/dh);
  auto p=&a.pixels[(sy*a.width+sx)*4];auto& dst=framebuffer()[py*WIDTH+px];int alpha=p[3];
  int r=(p[0]*alpha+((dst>>16)&255)*(255-alpha))/255,g=(p[1]*alpha+((dst>>8)&255)*(255-alpha))/255,b=(p[2]*alpha+(dst&255)*(255-alpha))/255;
  dst=0xff000000|(r<<16)|(g<<8)|b;
 }
 return true;
}
bool draw_art(const std::string& id,int x,int y,int w,int h){auto i=cache.find(id);return i!=cache.end()&&draw_image(i->second,x,y,w,h,false);}
bool draw_art_cover(const std::string& id,int x,int y,int w,int h){auto i=cache.find(id);return i!=cache.end()&&draw_image(i->second,x,y,w,h,true);}
void draw_brand_mark(int x,int y,int size){
 if(!mark_loaded){mark_loaded=true;int channels;auto p=stbi_load("/app0/assets/jellyfin-mark.png",&mark.width,&mark.height,&channels,4);if(!p)p=stbi_load("assets/jellyfin-mark.png",&mark.width,&mark.height,&channels,4);if(p){mark.pixels.assign(p,p+mark.width*mark.height*4);stbi_image_free(p);}}
 draw_image(mark,x,y,size,size,false);
}
''';f.write_text(t,newline='\n')
