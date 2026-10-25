#include "playback_model.h"
#include "json_lite.h"
#include "json_unescape.h"
#include <string.h>
static std::string string_field(const char* object,const char* key){char value[512]={};auto end=object+strlen(object);auto p=find_top_value(object,end,key);if(p&&*p=='"')json_unescape(p+1,end,value,sizeof value);return value;}
std::string select_playback_source(const std::string& json,const std::string& requested){
 auto end=json.c_str()+json.size();auto array=find_top_value(json.c_str(),end,"MediaSources");if(!array||*array!='[')return {};auto p=array+1;
 while(p<end){p=skip_ws(p,end);if(p>=end||*p==']')break;if(*p==','){++p;continue;}auto next=object_end(p,end);if(!next)break;std::string source(p,next);auto id=string_field(source.c_str(),"Id");auto token=string_field(source.c_str(),"OpenToken");auto path=string_field(source.c_str(),"Path");if(path.rfind("gelato://stub",0)!=0&&(requested.empty()||id==requested)&&(!id.empty()||!token.empty()||!string_field(source.c_str(),"TranscodingUrl").empty()))return source;p=next;
 }return {};
}
std::string playback_response_summary(const std::string& json){
 auto end=json.c_str()+json.size();auto p=find_top_value(json.c_str(),end,"MediaSources");int count=0;
 if(p&&*p=='['){++p;while(p<end){p=skip_ws(p,end);if(p>=end||*p==']')break;if(*p==','){++p;continue;}auto next=object_end(p,end);if(!next)break;++count;p=next;}}
 std::string error=string_field(json.c_str(),"ErrorCode");if(error.empty()){auto value=find_top_value(json.c_str(),end,"ErrorCode");if(value&&*value!='n')error=std::to_string(read_json_int(value,end,0));}
 return "PlaybackInfo: sources="+std::to_string(count)+" error="+(error.empty()?"none":error);
}
