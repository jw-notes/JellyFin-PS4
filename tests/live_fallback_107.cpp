#include "app.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv){
 assert(argc==2);snprintf(g_server,sizeof g_server,"%s",argv[1]);snprintf(g_userid,sizeof g_userid,"fixture-user");snprintf(g_token,sizeof g_token,"fixture-token");device_id="fixture-device";
 Item channel;snprintf(channel.base.id,sizeof channel.base.id,"channel-id");snprintf(channel.base.type,sizeof channel.base.type,"TvChannel");
 std::string url,session,source;assert(playback_url(channel,-1,-1,"",url,session,source));
 assert(source=="opened-source"&&session=="catalogue-session");
 for(const char* part:{"/Videos/channel-id/master.m3u8?","LiveStreamId=opened-live","MediaSourceId=opened-source","VideoCodec=h264","AudioCodec=aac","api_key=fixture-token"})assert(url.find(part)!=std::string::npos);
 end_playback(session);puts("PASS NoCompatibleStream recovery: real source catalogue -> OpenToken -> opened live source -> authenticated HLS endpoint -> query-based close");
}
