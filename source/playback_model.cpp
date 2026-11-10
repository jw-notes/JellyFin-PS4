// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
#include "playback_model.h"
#include "json_lite.h"
#include "json_unescape.h"
#include <string.h>
#include <ctype.h>
static std::string string_field(const char* object,const char* key){char value[512]={};auto end=object+strlen(object);auto p=find_top_value(object,end,key);if(p&&*p=='"')json_unescape(p+1,end,value,sizeof value);return value;}
std::string select_playback_source(const std::string& json,const std::string& requested){
 auto end=json.c_str()+json.size();auto array=find_top_value(json.c_str(),end,"MediaSources");if(!array||*array!='[')return {};auto p=array+1;
 while(p<end){p=skip_ws(p,end);if(p>=end||*p==']')break;if(*p==','){++p;continue;}auto next=object_end(p,end);if(!next)break;std::string source(p,next);auto id=string_field(source.c_str(),"Id");auto token=string_field(source.c_str(),"OpenToken");auto path=string_field(source.c_str(),"Path");if(path.rfind("gelato://stub",0)!=0&&(requested.empty()||id==requested)&&(!id.empty()||!token.empty()||!string_field(source.c_str(),"TranscodingUrl").empty()))return source;p=next;
 }return {};
}
// Each object in a source's MediaStreams.
template<class F> static void each_stream(const std::string& source,F visit){
 auto begin=source.c_str(),end=begin+source.size();auto p=find_top_value(begin,end,"MediaStreams");if(!p||*p!='[')return;++p;
 while(p<end){p=skip_ws(p,end);if(p>=end||*p==']')break;if(*p==','){++p;continue;}auto next=object_end(p,end);if(!next)break;visit(std::string(p,next));p=next;}
}
static int int_field(const char* object,const char* key,int fallback){auto end=object+strlen(object);auto p=find_top_value(object,end,key);return p&&*p!='n'?(int)read_json_int(p,end,fallback):fallback;}
static bool bool_field(const char* object,const char* key){auto end=object+strlen(object);auto p=find_top_value(object,end,key);return p&&read_json_bool(p,end);}
int playback_audio_ordinal(const std::string& source,int stream_index){
 int count=0,found=-1;
 each_stream(source,[&](const std::string& stream){
  if(string_field(stream.c_str(),"Type")!="Audio")return;
  const bool external=bool_field(stream.c_str(),"IsExternal");if(!external)++count;
  if(int_field(stream.c_str(),"Index",-1)==stream_index)found=external?-1:count;});
 return found;
}
PlaybackSubtitle playback_subtitle(const std::string& source,int stream_index){
 PlaybackSubtitle out;int count=0;
 each_stream(source,[&](const std::string& stream){
  if(string_field(stream.c_str(),"Type")!="Subtitle")return;
  const bool external=bool_field(stream.c_str(),"IsExternal");if(!external)++count;
  if(int_field(stream.c_str(),"Index",-1)!=stream_index)return;
  out.found=true;out.external=external;out.ordinal=external?-1:count;
  out.codec=string_field(stream.c_str(),"Codec");for(char& c:out.codec)c=(char)tolower((unsigned char)c);
  out.delivery=string_field(stream.c_str(),"DeliveryMethod");out.delivery_url=string_field(stream.c_str(),"DeliveryUrl");});
 return out;
}
int playback_default_audio(const std::string& source){return int_field(source.c_str(),"DefaultAudioStreamIndex",-1);}
std::string playback_url_param(const std::string& url,const std::string& name){
 const auto query=url.find('?');if(query==std::string::npos)return {};
 size_t at=query+1;
 while(at<url.size()){auto stop=url.find('&',at);if(stop==std::string::npos)stop=url.size();auto equals=url.find('=',at);
  if(equals!=std::string::npos&&equals<stop&&url.compare(at,equals-at,name)==0)return url.substr(equals+1,stop-equals-1);
  at=stop+1;}
 return {};
}
std::vector<std::string> playback_source_report(const std::string& source,int selected_audio){
 std::vector<std::string> lines;
 const std::string container=string_field(source.c_str(),"Container");
 lines.push_back("Source: container="+(container.empty()?std::string("unknown"):container)+" server_default_audio="+std::to_string(playback_default_audio(source))+" selected_audio="+std::to_string(selected_audio));
 each_stream(source,[&](const std::string& stream){
  const std::string type=string_field(stream.c_str(),"Type");if(type!="Audio"&&type!="Video")return;
  const int index=int_field(stream.c_str(),"Index",-1);
  std::string line="Source: stream "+std::to_string(index)+" "+type+" codec="+string_field(stream.c_str(),"Codec");
  if(type=="Audio"){
   const std::string layout=string_field(stream.c_str(),"ChannelLayout"),language=string_field(stream.c_str(),"Language");
   line+=" channels="+std::to_string(int_field(stream.c_str(),"Channels",0))+" layout="+(layout.empty()?std::string("unknown"):layout)+" rate="+std::to_string(int_field(stream.c_str(),"SampleRate",0))+" bitrate="+std::to_string(int_field(stream.c_str(),"BitRate",0))+" lang="+(language.empty()?std::string("none"):language);
   if(bool_field(stream.c_str(),"IsDefault"))line+=" default";
   if(bool_field(stream.c_str(),"IsExternal"))line+=" external";
   if(index==selected_audio)line+=" SELECTED";
  }else line+=" "+std::to_string(int_field(stream.c_str(),"Width",0))+"x"+std::to_string(int_field(stream.c_str(),"Height",0))+" bits="+std::to_string(int_field(stream.c_str(),"BitDepth",0));
  lines.push_back(line);});
 return lines;
}
std::string playback_response_summary(const std::string& json){
 auto end=json.c_str()+json.size();auto p=find_top_value(json.c_str(),end,"MediaSources");int count=0;
 if(p&&*p=='['){++p;while(p<end){p=skip_ws(p,end);if(p>=end||*p==']')break;if(*p==','){++p;continue;}auto next=object_end(p,end);if(!next)break;++count;p=next;}}
 std::string error=string_field(json.c_str(),"ErrorCode");if(error.empty()){auto value=find_top_value(json.c_str(),end,"ErrorCode");if(value&&*value!='n')error=std::to_string(read_json_int(value,end,0));}
 return "PlaybackInfo: sources="+std::to_string(count)+" error="+(error.empty()?"none":error);
}
