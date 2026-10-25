from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/app.cpp';t=f.read_text().replace('#include "auth.h"','#include "auth.h"\n#include "playback_model.h"');a=t.index(' std::string body=',t.index('bool playback_url('));b=t.index('\nstatic pthread_t report_thread;',a)
t=t[:a]+''' const bool live=!strcmp(i.base.type,"TvChannel");
 live_stream_id.clear();session.clear();source_id.clear();url.clear();
 std::string body="{\\\"UserId\\\":"+json_quote(g_userid)+",\\\"ItemId\\\":"+json_quote(i.base.id)+",\\\"DeviceProfile\\\":"+profile+",\\\"EnableDirectPlay\\\":false,\\\"EnableDirectStream\\\":false,\\\"EnableTranscoding\\\":true,\\\"AutoOpenLiveStream\\\":true,\\\"MaxAudioChannels\\\":2,\\\"MaxStreamingBitrate\\\":"+std::to_string(settings.bitrate)+",\\\"StartTimeTicks\\\":"+std::to_string(!live&&settings.resume?i.resume*10000:0)+",\\\"SubtitleStreamIndex\\\":"+std::to_string(sub);
 if(!selected_source.empty())body+=",\\\"MediaSourceId\\\":"+json_quote(selected_source);
 if(audio>=0)body+=",\\\"AudioStreamIndex\\\":"+std::to_string(audio);
 body+="}";
 const std::string endpoint="/Items/"+encode(i.base.id)+"/PlaybackInfo?UserId="+encode(g_userid)+"&AutoOpenLiveStream=true&IsPlayback=true";
 if(request(HTTP_POST,endpoint,body)!=200)return false;
 const std::string reply=responseBuffer;
 plog(playback_response_summary(reply).c_str());
 session=field(reply.c_str(),"PlaySessionId");
 std::string source=select_playback_source(reply,selected_source);
 if(source.empty()){
  std::string reason=field(reply.c_str(),"ErrorCode");
  last_error=(live?"The server returned no playable Live TV sources":"The server returned no playable media sources")+std::string(reason.empty()?".":" ("+reason+").");
  plog(last_error.c_str());return false;
 }
 auto end=source.c_str()+source.size();auto opening=find_top_value(source.c_str(),end,"RequiresOpening");
 auto token=field(source.c_str(),"OpenToken");
 if(opening&&read_json_bool(opening,end)&&field(source.c_str(),"LiveStreamId").empty()){
  if(token.empty()){last_error="The live source requires opening, but the server did not provide an open token.";plog(last_error.c_str());return false;}
  std::string open_body=body;open_body.pop_back();open_body+=",\\\"OpenToken\\\":"+json_quote(token)+",\\\"PlaySessionId\\\":"+json_quote(session)+"}";
  plog("Live TV: opening the server-provided live source");
  if(request(HTTP_POST,"/LiveStreams/Open",open_body)!=200)return false;
  auto opened_end=responseBuffer+strlen(responseBuffer);auto media=find_top_value(responseBuffer,opened_end,"MediaSource");auto media_end=media?object_end(media,opened_end):nullptr;
  if(!media_end){last_error="The server did not return an opened live source.";plog(last_error.c_str());return false;}
  source.assign(media,media_end);
 }
 source_id=field(source.c_str(),"Id");live_stream_id=field(source.c_str(),"LiveStreamId");url=field(source.c_str(),"TranscodingUrl");
 end=source.c_str()+source.size();auto support=find_top_value(source.c_str(),end,"SupportsDirectStream");
 plog((std::string("Playback source: id=")+source_id+" live="+(!live_stream_id.empty()?"yes":"no")+" transcoding_url="+(!url.empty()?"yes":"no")).c_str());
 if(settings.direct&&!live){url=std::string(g_server)+(strcmp(i.base.type,"Audio")==0?"/Audio/":"/Videos/")+encode(i.base.id)+"/stream?Static=true&MediaSourceId="+encode(source_id)+"&api_key="+encode(g_token);}
 else if(url.empty()){
  auto direct=field(source.c_str(),"DirectStreamUrl");
  if(!direct.empty()&&support&&read_json_bool(support,end))url=direct;
  else{last_error=live?"The live source opened, but the server did not offer a compatible H264/AAC stream.":"The server did not offer a compatible H264/AAC transcode.";plog(last_error.c_str());return false;}
 }
 if(!url.empty()&&url[0]=='/')url=std::string(g_server)+url;
 if(!settings.direct||live){url+=(url.find('?')==std::string::npos?"?":"&");url+="MaxWidth=1920&MaxHeight=1080&AudioSampleRate=48000&MaxAudioChannels=2";}
 if(url.rfind("http://",0)!=0&&url.rfind("https://",0)!=0){last_error="Unsupported playback URL";return false;}
 return true;
}
'''+t[b:]
a=t.index(' if(!live_stream_id.empty()){auto body=',t.index('void end_playback'));b=t.index('\n\nvoid app_shutdown',a)
t=t[:a]+''' if(!live_stream_id.empty()){http_request(HTTP_POST,(std::string(g_server)+"/LiveStreams/Close?LiveStreamId="+encode(live_stream_id)).c_str(),"",g_token,out,sizeof out);live_stream_id.clear();}}
'''+t[b:]
f.write_text(t,newline='\n')
f=p/'tools/build.sh';t=f.read_text().replace('source/core.cpp source/ui_view.cpp','source/core.cpp source/playback_model.cpp source/input_model.cpp source/input.cpp source/ui_view.cpp');f.write_text(t,newline='\n')
for name in ['test.sh','test_auth.sh','test_renderer.sh','test_keyboard.sh','test_ui_103.sh']:
 f=p/'tools'/name;t=f.read_text().replace('source/app.cpp source/auth.cpp','source/app.cpp source/playback_model.cpp source/auth.cpp');f.write_text(t,newline='\n')
