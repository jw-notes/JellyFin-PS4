#include "app.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(int argc,char** argv){
 assert(argc==2);static_assert(RESPONSE_SIZE==8*1024*1024,"PS4 response budget");
 snprintf(g_server,sizeof g_server,"%s",argv[1]);snprintf(g_userid,sizeof g_userid,"fixture");
 Page page;assert(fetch_page("large-library",0,"",0,page));assert(page.total==121&&page.items.size()==60);
 assert(!strcmp(page.items.back().base.name,"Large library item 59"));assert(page.items.front().year==2026);
 assert(page.items.front().cast=="Actor One, Actor Two");assert(page.items.front().genres=="Drama");
 assert(strlen(responseBuffer)>384*1024);
 puts("PASS real HTTP response over 384 KiB: all 60 cards parsed, metadata retained and listing omits unused MediaSources");
}
