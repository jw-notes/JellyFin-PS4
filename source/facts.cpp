#include "facts.h"
#include <map>
#include <string.h>
#include <set>
#include <deque>
#include <pthread.h>
struct FactsJob{Item item;std::string url,token;};
struct FactsResult{Item item;std::string status;};
static std::map<std::string,FactsResult> cache;
static std::set<std::string> pending;
static std::deque<FactsJob> jobs;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static pthread_t worker;static bool running=false,stopping=false;
static void* fetch_facts(void*){
 for(;;){pthread_mutex_lock(&lock);while(jobs.empty()&&!stopping)pthread_cond_wait(&changed,&lock);if(stopping){pthread_mutex_unlock(&lock);return nullptr;}auto job=jobs.front();jobs.pop_front();pthread_mutex_unlock(&lock);
  std::vector<uint8_t> body(RESPONSE_SIZE);Item detail;std::string status;
  int count=http_fetch_binary(job.url.c_str(),job.token.c_str(),body.data(),(int)body.size());
  if(count>0&&parse_preview_item((const char*)body.data(),detail)&&job.item.base.id==std::string(detail.base.id)){
   if(detail.overview.empty())detail.overview=job.item.overview;
   if(detail.cast.empty())detail.cast=job.item.cast;
   plog("Quick peek: detail metadata loaded");
  }else{detail=job.item;status="Could not load item details.";plog("Quick peek: detail request failed");}
  pthread_mutex_lock(&lock);pending.erase(job.item.base.id);if(!stopping){if(cache.size()>=32)cache.erase(cache.begin());cache[job.item.base.id]={detail,status};}pthread_mutex_unlock(&lock);
 }
}
void facts_request(const Item& item){
 if(!item.base.id[0]||!strncmp(item.base.id,"nav:",4))return;
 pthread_mutex_lock(&lock);auto previous=cache.find(item.base.id);if((previous!=cache.end()&&previous->second.status.empty())||pending.count(item.base.id)){pthread_mutex_unlock(&lock);return;}if(previous!=cache.end())cache.erase(previous);
 if(!running){if(pthread_create(&worker,nullptr,fetch_facts,nullptr)){pthread_mutex_unlock(&lock);return;}running=true;}
 while(jobs.size()>=4){pending.erase(jobs.front().item.base.id);jobs.pop_front();}
 pending.insert(item.base.id);jobs.push_back({item,std::string(g_server)+"/Users/"+encode(g_userid)+"/Items/"+encode(item.base.id)+"?Fields=Overview,People,Genres",g_token});pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);
}
bool facts_copy(const std::string& id,Item& item,std::string& status){pthread_mutex_lock(&lock);auto found=cache.find(id);bool ready=found!=cache.end();if(ready){item=found->second.item;status=found->second.status;}else status="Loading item details...";pthread_mutex_unlock(&lock);return ready;}
void facts_stop(bool clear_cache){pthread_mutex_lock(&lock);bool join=running;stopping=true;jobs.clear();pending.clear();pthread_cond_broadcast(&changed);pthread_mutex_unlock(&lock);if(join)pthread_join(worker,nullptr);pthread_mutex_lock(&lock);running=false;stopping=false;if(clear_cache)cache.clear();pthread_mutex_unlock(&lock);}
