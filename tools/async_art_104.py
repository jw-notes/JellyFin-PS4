from pathlib import Path
p=Path(r'D:\Coding-Projects\PS4\JellyFin-PS4')
f=p/'source/ui_view.h';t=f.read_text().replace('int library_first(int selected,bool home,int focused_tab=-1);','int library_first(int selected,bool home);');f.write_text(t,newline='\n')
f=p/'source/ui_view.cpp';t=f.read_text().replace('int library_first(int selected,bool home,int focused_tab){','int library_first(int selected,bool home){');f.write_text(t,newline='\n')
f=p/'source/http_ps4.cpp';t=f.read_text().replace('extern "C" int32_t sceSslTerm(int32_t);', 'extern "C" int32_t sceSslTerm(int32_t);\nextern "C" int32_t bounded_http_receive(int32_t,uint32_t) __asm__("sceHttpSetRecvTimeOut");')
t=t.replace('int cap,int* count){','int cap,int* count,bool artwork=false){').replace('    if(req>=0){\n', '    if(req>=0){\n        if(artwork){sceHttpSetConnectTimeOut(req,4000000);sceHttpSetResolveTimeOut(req,2000000);sceHttpSetSendTimeOut(req,4000000);bounded_http_receive(req,4000000);}\n')
t=t.replace('(char*)out,cap,&count);','(char*)out,cap,&count,true);');f.write_text(t,newline='\n')
f=p/'source/art.h';t=f.read_text()+'\nvoid art_wait();\nvoid art_stop(bool clear_cache=true);\n';f.write_text(t,newline='\n')
f=p/'source/art.cpp';t=f.read_text().replace('#include <algorithm>','#include <algorithm>\n#include <deque>\n#include <set>\n#include <pthread.h>')
a=t.index('void load_art(');b=t.index('\nstatic bool draw_image',a)
t=t[:a]+'''struct ArtJob{std::string id,url,token;};
static pthread_mutex_t art_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t art_changed=PTHREAD_COND_INITIALIZER;
static std::deque<ArtJob> art_queue;
static std::set<std::string> pending;
static pthread_t art_thread;
static bool art_running=false,art_stopping=false,art_fetching=false;
static void* artwork_worker(void*){
 for(;;){
  pthread_mutex_lock(&art_lock);
  while(art_queue.empty()&&!art_stopping)pthread_cond_wait(&art_changed,&art_lock);
  if(art_stopping){pthread_mutex_unlock(&art_lock);return nullptr;}
  ArtJob job=art_queue.front();art_queue.pop_front();art_fetching=true;pthread_mutex_unlock(&art_lock);
  Art art;std::vector<uint8_t> buffer(512*1024);int size=http_fetch_binary(job.url.c_str(),job.token.c_str(),buffer.data(),(int)buffer.size());
  if(size>0){int channels;auto data=stbi_load_from_memory(buffer.data(),size,&art.width,&art.height,&channels,4);if(data){art.pixels.assign(data,data+art.width*art.height*4);stbi_image_free(data);}}
  pthread_mutex_lock(&art_lock);
  if(!art_stopping){if(cache.size()>=64){auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});cache.erase(oldest);}art.used=++counter;cache[job.id]=std::move(art);}
  pending.erase(job.id);art_fetching=false;pthread_cond_broadcast(&art_changed);pthread_mutex_unlock(&art_lock);
 }
}
void load_art(const std::string& id){
 if(id.empty())return;
 pthread_mutex_lock(&art_lock);
 auto existing=cache.find(id);if(existing!=cache.end()){existing->second.used=++counter;pthread_mutex_unlock(&art_lock);return;}
 if(pending.count(id)){pthread_mutex_unlock(&art_lock);return;}
 if(!art_running){art_stopping=false;if(pthread_create(&art_thread,nullptr,artwork_worker,nullptr)){pthread_mutex_unlock(&art_lock);plog("Artwork worker could not start");return;}art_running=true;}
 while(art_queue.size()>=24){pending.erase(art_queue.front().id);art_queue.pop_front();}
 pending.insert(id);art_queue.push_back({id,std::string(g_server)+"/Items/"+encode(id)+"/Images/Primary?MaxWidth=320&Quality=85",g_token});pthread_cond_signal(&art_changed);pthread_mutex_unlock(&art_lock);
}
void art_wait(){pthread_mutex_lock(&art_lock);while((art_fetching||!art_queue.empty())&&!art_stopping)pthread_cond_wait(&art_changed,&art_lock);pthread_mutex_unlock(&art_lock);}
void art_stop(bool clear_cache){
 pthread_mutex_lock(&art_lock);bool join=art_running;art_stopping=true;art_queue.clear();pending.clear();pthread_cond_broadcast(&art_changed);pthread_mutex_unlock(&art_lock);
 if(join)pthread_join(art_thread,nullptr);
 pthread_mutex_lock(&art_lock);art_running=false;art_stopping=false;art_fetching=false;if(clear_cache)cache.clear();pthread_mutex_unlock(&art_lock);
}
'''+t[b:]
t=t.replace('bool draw_art(const std::string& id,int x,int y,int w,int h){auto i=cache.find(id);return i!=cache.end()&&draw_image(i->second,x,y,w,h,false);}', 'bool draw_art(const std::string& id,int x,int y,int w,int h){pthread_mutex_lock(&art_lock);auto i=cache.find(id);bool result=i!=cache.end()&&draw_image(i->second,x,y,w,h,false);pthread_mutex_unlock(&art_lock);return result;}')
t=t.replace('bool draw_art_cover(const std::string& id,int x,int y,int w,int h){auto i=cache.find(id);return i!=cache.end()&&draw_image(i->second,x,y,w,h,true);}', 'bool draw_art_cover(const std::string& id,int x,int y,int w,int h){pthread_mutex_lock(&art_lock);auto i=cache.find(id);bool result=i!=cache.end()&&draw_image(i->second,x,y,w,h,true);pthread_mutex_unlock(&art_lock);return result;}')
f.write_text(t,newline='\n')
f=p/'source/main.cpp';t=f.read_text().replace('logout();return;', 'art_stop();logout();return;').replace('authenticated=sign_in();','art_stop();authenticated=sign_in();').replace('}player_close();app_shutdown();','}player_close();art_stop();app_shutdown();');f.write_text(t,newline='\n')
f=p/'tests/ui_preview_103.cpp';t=f.read_text().replace(' Page library;library.total=6;',' art_wait();\n Page library;library.total=6;').replace(' puts("PASS shared', ' art_stop();\n puts("PASS shared');f.write_text(t,newline='\n')
