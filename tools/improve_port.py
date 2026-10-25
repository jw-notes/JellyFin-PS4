from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/platform_ps4.cpp';t=f.read_text().replace('#include <algorithm>','#include <algorithm>\n#include <map>').replace('#include "opensans_regular.h"','#include "opensans_regular.h"\n#include "notosansjp.h"').replace('static stbtt_fontinfo font;','static stbtt_fontinfo font,japanese_font;\nstatic std::map<unsigned,Glyph> *unused_glyph_declaration;')
# Define the Unicode cache after Glyph has been defined.
t=t.replace('static std::map<unsigned,Glyph> *unused_glyph_declaration;','');t=t.replace('static Glyph glyphs[2][128];','static Glyph glyphs[2][128];\nstatic std::map<unsigned,Glyph> unicode_glyphs[2];')
t=t.replace('stbtt_InitFont(&font,OpenSans_Regular_ttf,0);','stbtt_InitFont(&font,OpenSans_Regular_ttf,0);\n stbtt_InitFont(&japanese_font,NotoSansJP_otf,0);')
old="unsigned char c=str[i];if(c>=128){if((c&0xc0)==0x80)continue;c='?';}if(c<32)continue;auto&g=glyphs[tier][c];"
new='''unsigned c=(unsigned char)str[i];if(c>=128){unsigned extra=(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:(c&0xf8)==0xf0?3:0;c&=(1u<<(6-extra))-1;for(unsigned j=0;j<extra&&i+1<str.size();j++){unsigned next=(unsigned char)str[i+1];if((next&0xc0)!=0x80){c='?';break;}c=(c<<6)|(next&63);i++;}}if(c<32)continue;
 Glyph* glyph=nullptr;
 if(c<128)glyph=&glyphs[tier][c];else{auto& cache=unicode_glyphs[tier];auto it=cache.find(c);if(it==cache.end()){if(cache.size()>=1024){for(auto& entry:cache)stbtt_FreeBitmap(entry.second.data,nullptr);cache.clear();}Glyph g{};auto f=stbtt_FindGlyphIndex(&font,c)?&font:&japanese_font;float scale=stbtt_ScaleForPixelHeight(f,tier?32:24);int advance;stbtt_GetCodepointHMetrics(f,c,&advance,nullptr);g.advance=(int)(advance*scale);g.data=stbtt_GetCodepointBitmap(f,0,scale,c,&g.w,&g.h,&g.x,&g.y);it=cache.emplace(c,g).first;}glyph=&it->second;}
 auto&g=*glyph;'''
assert old in t;t=t.replace(old,new);f.write_text(t)
f=p/'source/app.cpp';t=f.read_text().replace('#include <time.h>','#include <time.h>\n#include <pthread.h>\n#include <atomic>')
t=t.replace('Settings settings;','static std::string live_stream_id;\nSettings settings;')
t=t.replace('source_id=field(obj.c_str(),"Id");url=', 'source_id=field(obj.c_str(),"Id");live_stream_id=field(obj.c_str(),"LiveStreamId");url=')
a=t.index('void report_playback(');t=t[:a]+'''static pthread_t report_thread;
static bool report_joinable=false;
static std::atomic<bool> report_busy{false};
struct ReportJob {std::string url,body,token;};
static void* send_report(void* p){auto job=(ReportJob*)p;char response[512];http_request(HTTP_POST,job->url.c_str(),job->body.c_str(),job->token.c_str(),response,sizeof response);delete job;report_busy=false;return nullptr;}
static void flush_reports(){if(report_joinable){pthread_join(report_thread,nullptr);report_joinable=false;}}
void report_playback(const char* event,const Item& i,const std::string& session,const std::string& source,uint64_t ms,bool paused){
 bool live=!strcmp(i.base.type,"TvChannel");
 std::string body="{\\\"ItemId\\\":"+json_quote(i.base.id)+",\\\"MediaSourceId\\\":"+json_quote(source)+",\\\"PlaySessionId\\\":"+json_quote(session)+",\\\"PositionTicks\\\":"+std::to_string(live?0:ms*10000)+",\\\"IsPaused\\\":"+(paused?"true":"false")+",\\\"CanSeek\\\":"+(live?"false":"true")+",\\\"PlayMethod\\\":\\\""+(settings.direct?"DirectPlay":"Transcode")+"\\\"";
 if(!live_stream_id.empty())body+=",\\\"LiveStreamId\\\":"+json_quote(live_stream_id);
 body+="}";
 std::string url=std::string(g_server)+"/Sessions/Playing"+event;
 if(!strcmp(event,"/Progress")){
  if(report_busy)return;
  flush_reports();auto job=new ReportJob{url,body,g_token};report_busy=true;
  if(pthread_create(&report_thread,nullptr,send_report,job)==0)report_joinable=true;
  else{delete job;report_busy=false;plog("Unable to start progress report worker");}
 }else{flush_reports();char small[512];http_request(HTTP_POST,url.c_str(),body.c_str(),g_token,small,sizeof small);}
}
void end_playback(const std::string& session){flush_reports();char out[512];http_request(HTTP_DELETE,(std::string(g_server)+"/Videos/ActiveEncodings?DeviceId="+encode(device_id)+"&PlaySessionId="+encode(session)).c_str(),"",g_token,out,sizeof out);
 if(!live_stream_id.empty()){auto body="{\\\"Id\\\":"+json_quote(live_stream_id)+"}";http_request(HTTP_POST,(std::string(g_server)+"/LiveStreams/Close").c_str(),body.c_str(),g_token,out,sizeof out);live_stream_id.clear();}}
''';f.write_text(t)
f=p/'source/main.cpp';t=f.read_text().replace('if(b&ORBIS_PAD_BUTTON_LEFT)player_seek','if(b&ORBIS_PAD_BUTTON_LEFT&&strcmp(item.base.type,"TvChannel"))player_seek').replace('if(b&ORBIS_PAD_BUTTON_RIGHT)player_seek','if(b&ORBIS_PAD_BUTTON_RIGHT&&strcmp(item.base.type,"TvChannel"))player_seek');f.write_text(t)
