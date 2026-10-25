// The Live TV page in the Classic layout, ported from the PS3's
// vendor/ps3/source/ui/xmb/ui_livetv.cpp. The requests, parsing, sorting,
// categories and the viewer's page preferences are the PS3's own files
// (api/api_livetv.cpp, api/livetv.cpp, api/livetv_prefs.c), unchanged.
//
//   * a worker thread does every request, so the screen never waits on the
//     network: the channel-number index (which categories exist), one page of
//     channels, or the guide for the rows on screen.
//   * the list refreshes every 60 s and again after watching.
//   * more than 200 channels are shown a category at a time (L2 / R2), plus
//     Favourites, Recently watched and genre pages guessed from the names.
//   * the grid is the PS3 Classic one: three rows of wide channel cards with
//     the logo, the programme's progress, the number, name and what is on.
//   * Square opens the three-hour guide; X on a later programme shows its
//     details. The touchpad searches channels by name.
#include "livetv_view.h"
#include "platform.h"
#include "ui_view.h"
#include "art.h"
#include "input_model.h"
#include "api_livetv.h"
#include "livetv.h"
#include "livetv_prefs.h"
#include "api_userdata.h"
#include <orbis/_types/pad.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

namespace {

const uint64_t T_MIN=600000000ULL,T_30M=30*T_MIN,T_3H=180*T_MIN;
const int MAX_CH=LIVETV_MAX_CHANNELS,MAX_INDEX=6000,PAGE_MIN=200,GENRE_MIN=3,MAX_PAGES=48,MAX_PG=400,GUIDE_ROWS=16;
const uint64_t REFRESH_MS=60000,DEBOUNCE_MS=250;

// Categories: blocks of channel numbers, as the PS3 names them.
struct CatDef{int base;const char* label;};
const CatDef CATS[]={{1,"All Channels"},{101,"NFL"},{151,"NBA"},{201,"NHL"},{251,"MLB"},{331,"Motorsport"},{351,"Combat"},{371,"Regional teams"},{421,"International"},{471,"ESPN+"},{651,"PPV"},{671,"WNBA"},{1000,"News"},{2000,"Entertainment"},{3000,"Movies"},{4000,"Kids"},{5000,"Music"},{6000,"Canadian"},{7000,"Latino"},{8000,"24/7"},{9000,"Local (US)"},{19000,"Other"}};
const int N_CATS=(int)(sizeof CATS/sizeof *CATS),SPORTS_BELOW=1000;
const char* const GENRE_LABEL[LG_COUNT]={"News","Sports","Movies","Kids","Music","Crime & Mystery","Comedy","Horror & Sci-Fi","Reality","Food","Home & Lifestyle","Nature & Animals","History & Learning","Game Shows","Westerns","Cars & Motors","Faith","Latino","Shopping"};

uint64_t now_ms(){timespec ts{};clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*1000+ts.tv_nsec/1000000;}
uint64_t floor_half_hour(uint64_t t){return t-(t%T_30M);}

// ---- state (screen thread) ----
std::vector<JFChannel> ch;int n=0;
std::vector<JFProgram> pg;int pn=0,pg_first=0,pg_rows=0;uint64_t pg_win0=0;
bool loaded=false,failed=false;int utc_offset=0;
int sel=0,top=0;bool guide=false;uint64_t gt=0,base_win0=0,win0=0;
uint64_t last_refresh=0,view_changed=0;int seen_top=-1,seen_vis=-1;uint64_t seen_win0=0;
int gcount[LG_COUNT];
struct Page{int first,count,cat,key;};
Page pages[MAX_PAGES];int npages=0,raw_n=0,page=0;
int raw_first[MAX_PAGES],raw_count[MAX_PAGES],raw_cat[MAX_PAGES];
LtPrefs prefs;bool prefs_loaded=false;
std::vector<float> nums;int nnums=0;bool index_ok=false,index_tried=false;
bool search_on=false;std::string term;int gen=0;
bool paging(){return index_ok&&npages>1;}

// ---- the worker ----
enum{J_IDLE=0,J_QUEUED,J_RUNNING,J_DONE};
enum{JOB_CHANNELS=1,JOB_GUIDE=2,JOB_INDEX=4};
struct Job{int kind=0,cfirst=0,ccount=0,gen=0,mode=0,genre=0,n_recent=0;char recent[LP_MAX_RECENT][LP_ID_MAX]{};std::string term;int first=0,rows=0,n_ids=0;char ids[GUIDE_ROWS][40]{};uint64_t w0=0,w1=0;};
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
int jstate=J_IDLE;Job job;
std::vector<JFChannel> w_ch;int w_n=0,w_ctotal=0,w_nnums=0,w_pn=0;bool w_ch_ok=false,w_idx_ok=false;
std::vector<float> w_nums;int w_gcount[LG_COUNT];std::vector<JFProgram> w_pg;
pthread_t thread;bool running=false,stopping=false;

void* worker(void*){
 for(;;){
  pthread_mutex_lock(&lock);while(jstate!=J_QUEUED&&!stopping)pthread_cond_wait(&changed,&lock);
  if(stopping){pthread_mutex_unlock(&lock);return nullptr;}
  jstate=J_RUNNING;const Job j=job;pthread_mutex_unlock(&lock);
  w_ch_ok=w_idx_ok=false;w_pn=0;
  if(j.kind&JOB_INDEX){
   w_nnums=jf_fetch_channel_numbers(w_nums.data(),MAX_INDEX,nullptr);w_idx_ok=w_nnums>=0;memset(w_gcount,0,sizeof w_gcount);
   // The genre pages need every name once: an M3U list carries no categories.
   if(w_idx_ok){int total=0;int got=jf_fetch_channels_range(w_ch.data(),MAX_CH,0,MAX_CH,&total,nullptr);for(int i=0;i<got;++i){unsigned m=livetv_genre_mask(w_ch[i].name);for(int g=0;g<LG_COUNT;++g)if(m>>g&1u)w_gcount[g]++;}}
  }
  if(j.kind&JOB_CHANNELS){
   int total=0,got;
   if(j.mode==1)got=jf_fetch_channels_range(w_ch.data(),MAX_CH,0,MAX_CH,&total,nullptr,"&isFavorite=true");
   else if(j.mode==3){got=jf_fetch_channels_range(w_ch.data(),MAX_CH,0,MAX_CH,&total,nullptr);int k=0;for(int i=0;i<got;++i)if(livetv_genre_mask(w_ch[i].name)>>j.genre&1u)w_ch[k++]=w_ch[i];if(got>=0)got=k;}
   else if(j.mode==2){
    if(j.n_recent<=0)got=0;
    else{std::string extra="&Ids=";for(int i=0;i<j.n_recent;++i){if(i)extra+=",";extra+=j.recent[i];}
     got=jf_fetch_channels_range(w_ch.data(),MAX_CH,0,MAX_CH,&total,nullptr,extra.c_str());
     auto rank=[&](const JFChannel& c){for(int i=0;i<j.n_recent;++i)if(!strcmp(j.recent[i],c.id))return i;return (int)LP_MAX_RECENT;};
     if(got>0)std::stable_sort(w_ch.begin(),w_ch.begin()+got,[&](const JFChannel& a,const JFChannel& b){return rank(a)<rank(b);});}
   }
   else got=jf_fetch_channels_range(w_ch.data(),MAX_CH,j.cfirst,j.ccount,&total,j.term.empty()?nullptr:j.term.c_str());
   if(got>=0){w_n=got;w_ctotal=total;w_ch_ok=true;}
  }
  if(j.kind&JOB_GUIDE){const char* ids[GUIDE_ROWS];for(int i=0;i<j.n_ids;++i)ids[i]=j.ids[i];w_pn=jf_fetch_programs(ids,j.n_ids,j.w0,j.w1,w_pg.data(),MAX_PG);}
  pthread_mutex_lock(&lock);jstate=J_DONE;pthread_mutex_unlock(&lock);
 }
}
bool ensure_worker(){
 if(running)return true;
 ch.resize(MAX_CH);w_ch.resize(MAX_CH);pg.resize(MAX_PG);w_pg.resize(MAX_PG);nums.resize(MAX_INDEX);w_nums.resize(MAX_INDEX);
 stopping=false;jstate=J_IDLE;running=pthread_create(&thread,nullptr,worker,nullptr)==0;if(!running)plog("livetv: worker could not start");return running;
}

// ---- geometry: the PS3 Classic numbers at 720p ----
const int BAND_TOP=156,BAND_BOTTOM=650,TEXT_H=50,GAP_X=24;
struct LiveGrid{int cols,rows,card_w,card_h,stride,x0;};
LiveGrid live_grid(){LiveGrid g;int usable_w=WIDTH-128,usable_h=BAND_BOTTOM-BAND_TOP;g.rows=3;g.stride=usable_h/g.rows;g.card_h=g.stride-TEXT_H-10;g.card_w=g.card_h*16/9;g.cols=std::max(1,std::min(12,(usable_w+GAP_X)/(g.card_w+GAP_X)));g.x0=(WIDTH-(g.cols*g.card_w+(g.cols-1)*GAP_X))/2;return g;}
int card_x(const LiveGrid& g,int rel){return g.x0+(rel%g.cols)*(g.card_w+GAP_X);}
int card_y(const LiveGrid& g,int rel){return BAND_TOP+(rel/g.cols)*g.stride;}
const int GUIDE_X0=48,GUIDE_LOGO_W=150,GUIDE_HEAD_H=30,GUIDE_ROW_H=54,GUIDE_PITCH=60;
int guide_cells_x(){return GUIDE_X0+GUIDE_LOGO_W+10;}
int guide_cells_w(){return WIDTH-48-guide_cells_x();}
int guide_top(){return BAND_TOP+GUIDE_HEAD_H;}
int guide_rows_visible(){return std::max(1,(BAND_BOTTOM-guide_top()+6)/GUIDE_PITCH);}
int visible_count(){return guide?guide_rows_visible():live_grid().cols*live_grid().rows;}
int x_of_time(uint64_t t){if(t<=win0)return guide_cells_x();if(t>=win0+T_3H)return guide_cells_x()+guide_cells_w();return guide_cells_x()+(int)((double)(t-win0)/(double)T_3H*guide_cells_w());}
void fmt_hm(uint64_t ticks,char* out,int cap){livetv_format_hm(ticks,utc_offset,out,cap);}

int progs_of(const char* id,const JFProgram** out,int max){
 int k=0;for(int i=0;i<pn&&k<max;++i){if(strcmp(pg[i].channel_id,id)||pg[i].end_ticks<=pg[i].start_ticks)continue;int j=k++;while(j>0&&out[j-1]->start_ticks>pg[i].start_ticks){out[j]=out[j-1];--j;}out[j]=&pg[i];}return k;
}
struct NowNext{std::string now,next;uint64_t now_start=0,now_end=0,next_start=0;};
NowNext now_next(const JFChannel& c,uint64_t now){
 NowNext o;const JFProgram* ps[24];int k=progs_of(c.id,ps,24),cur=-1;for(int i=0;i<k;++i)if(ps[i]->start_ticks<=now&&now<ps[i]->end_ticks)cur=i;
 if(cur>=0){o.now=ps[cur]->name;o.now_start=ps[cur]->start_ticks;o.now_end=ps[cur]->end_ticks;for(int i=cur+1;i<k;++i)if(ps[i]->start_ticks>=ps[cur]->end_ticks){o.next=ps[i]->name;o.next_start=ps[i]->start_ticks;break;}}
 else if(c.now_title[0]&&c.now_start_ticks<=now&&now<c.now_end_ticks){o.now=c.now_title;o.now_start=c.now_start_ticks;o.now_end=c.now_end_ticks;for(int i=0;i<k;++i)if(ps[i]->start_ticks>=c.now_end_ticks){o.next=ps[i]->name;o.next_start=ps[i]->start_ticks;break;}}
 return o;
}
bool have_guide(){if(pn>0)return true;for(int i=0;i<n;++i)if(ch[i].now_title[0])return true;return false;}

// ---- jobs ----
int cur_key(){return paging()&&!search_on&&page<npages?pages[page].key:0;}
void submit(int kind){
 pthread_mutex_lock(&lock);if(jstate!=J_IDLE){pthread_mutex_unlock(&lock);return;}
 job=Job{};job.kind=kind;job.gen=gen;
 if(search_on){job.term=term;job.cfirst=0;job.ccount=MAX_CH;}
 else if(paging()&&page<npages){const int key=pages[page].key;
  if(key==LP_FAVS)job.mode=1;else if(key==LP_RECENT){job.mode=2;job.n_recent=prefs.n_recent;memcpy(job.recent,prefs.recent,sizeof job.recent);}
  else if(LP_IS_GENRE(key)){job.mode=3;job.genre=LP_GENRE_OF(key);}else{job.cfirst=pages[page].first;job.ccount=pages[page].count;}}
 else{job.cfirst=0;job.ccount=MAX_CH;}
 if(kind&JOB_GUIDE){const int vis=visible_count();job.first=top;job.rows=std::min(GUIDE_ROWS,std::max(vis,0));for(int i=0;i<job.rows&&top+i<n;++i){snprintf(job.ids[i],40,"%s",ch[top+i].id);job.n_ids++;}job.w0=win0;job.w1=win0+T_3H;}
 jstate=J_QUEUED;pthread_cond_signal(&changed);pthread_mutex_unlock(&lock);
}
void prefs_load(){if(prefs_loaded)return;prefs_loaded=true;lp_init(&prefs);FILE* f=fopen(jf_data_path("livetv.txt"),"r");if(!f)return;char buf[2048];size_t k=fread(buf,1,sizeof buf-1,f);fclose(f);buf[k]=0;lp_parse(buf,&prefs);}
void prefs_save(){char buf[2048];if(lp_format(&prefs,buf,sizeof buf)<0)return;FILE* f=fopen(jf_data_path("livetv.txt"),"w");if(!f)return;fputs(buf,f);fclose(f);}
void build_pages(int want){
 int bases[MAX_PAGES],keys[MAX_PAGES],genres[LG_COUNT],ng=0;for(int i=0;i<raw_n;++i)bases[i]=CATS[raw_cat[i]].base;
 for(int g=0;g<LG_COUNT;++g)if(gcount[g]>=GENRE_MIN)genres[ng++]=LP_GENRE(g);
 const int k=lp_build_pages_ex(&prefs,bases,raw_n,genres,ng,keys,MAX_PAGES);npages=0;
 for(int i=0;i<k;++i){Page& p=pages[npages++];p.key=keys[i];p.first=p.count=0;p.cat=-1;for(int r=0;r<raw_n;++r)if(keys[i]>0&&bases[r]==keys[i]){p.first=raw_first[r];p.count=raw_count[r];p.cat=raw_cat[r];}}
 page=lp_pick_page(keys,k,want?want:prefs.last);
}
void note_recent(const char* id){prefs_load();int before=prefs.n_recent;lp_note_recent(&prefs,id);prefs_save();if(before==0&&index_ok&&npages>0)build_pages(pages[page].key);}
void collect(){
 pthread_mutex_lock(&lock);if(jstate!=J_DONE){pthread_mutex_unlock(&lock);return;}const Job j=job;pthread_mutex_unlock(&lock);
 if((j.kind&JOB_INDEX)&&w_idx_ok){
  const int key_was=index_ok&&page<npages?pages[page].key:0;std::copy(w_nums.begin(),w_nums.begin()+w_nnums,nums.begin());nnums=w_nnums;
  int bases[N_CATS];for(int i=0;i<N_CATS;++i)bases[i]=CATS[i].base;raw_n=livetv_page_ranges(nums.data(),nnums,bases,N_CATS,raw_first,raw_count,raw_cat,MAX_PAGES);
  // A short list is one numbered page: All Channels.
  if(nnums<=PAGE_MIN){raw_n=nnums>0?1:0;raw_first[0]=0;raw_count[0]=nnums;raw_cat[0]=0;}
  memcpy(gcount,w_gcount,sizeof gcount);prefs_load();build_pages(key_was);index_ok=true;
  char b[80];snprintf(b,sizeof b,"livetv: %d channel(s) in %d page(s)",nnums,npages);plog(b);
  const int key_now=page<npages?pages[page].key:0;if(!search_on&&(key_now!=key_was||!loaded)){gen++;n=0;loaded=false;sel=top=0;}
  last_refresh=0;
 }
 if((j.kind&JOB_CHANNELS)&&j.gen!=gen)last_refresh=0;
 else if(j.kind&JOB_CHANNELS){
  if(w_ch_ok){std::string keep=sel>=0&&sel<n?ch[sel].id:"";std::copy(w_ch.begin(),w_ch.begin()+w_n,ch.begin());n=w_n;loaded=true;failed=false;
   for(int i=0;i<n;++i)if(!keep.empty()&&keep==ch[i].id){sel=i;break;}if(sel>=n)sel=n>0?n-1:0;pg_rows=0;
   if(index_ok&&j.term.empty()&&j.mode==0&&w_ctotal!=nnums)index_tried=false;}
  else if(!loaded)failed=true;
  last_refresh=now_ms();
 }
 if((j.kind&JOB_GUIDE)&&j.gen==gen){std::copy(w_pg.begin(),w_pg.begin()+w_pn,pg.begin());pn=w_pn;pg_first=j.first;pg_rows=j.rows;pg_win0=j.w0;}
 pthread_mutex_lock(&lock);jstate=J_IDLE;pthread_mutex_unlock(&lock);
}
bool guide_current(){if(pg_rows<=0||pg_win0!=win0)return false;const int vis=visible_count();return top>=pg_first&&top+vis<=pg_first+pg_rows;}
void tick(){
 collect();const uint64_t now=now_ms();
 // The window follows the clock until the guide is being read.
 if(!guide){const uint64_t base=floor_half_hour(jf_now_ticks());if(base!=base_win0){base_win0=base;win0=base;}}
 const int vis=visible_count();if(seen_top!=top||seen_vis!=vis||seen_win0!=win0){seen_top=top;seen_vis=vis;seen_win0=win0;view_changed=now;}
 pthread_mutex_lock(&lock);bool idle=jstate==J_IDLE;pthread_mutex_unlock(&lock);if(!idle)return;
 if(!index_tried){index_tried=true;submit(JOB_INDEX);return;}
 if(last_refresh==0||now-last_refresh>=REFRESH_MS){if(last_refresh==0)last_refresh=now;submit(JOB_CHANNELS);return;}
 if(n>0&&!guide_current()&&now-view_changed>=DEBOUNCE_MS)submit(JOB_GUIDE);
}
void list_changed(){gen++;n=0;pn=0;loaded=failed=false;sel=top=0;pg_rows=0;last_refresh=0;}
void set_page(int p){if(npages<=1)return;if(p<0)p=npages-1;if(p>=npages)p=0;if(p==page)return;page=p;prefs.last=pages[page].key;prefs_save();list_changed();}
std::string cat_label(int cat){const CatDef& c=CATS[cat];return c.base>1&&c.base<SPORTS_BELOW?std::string("Sports | ")+c.label:c.label;}
std::string key_label(int key,int cat){if(LP_IS_GENRE(key)&&LP_GENRE_OF(key)<LG_COUNT)return GENRE_LABEL[LP_GENRE_OF(key)];if(key==LP_FAVS)return "Favourites";if(key==LP_RECENT)return "Recently watched";return cat>=0?cat_label(cat):"Channels";}
void keep_visible(){
 if(!guide){const LiveGrid g=live_grid();int top_row=top/g.cols;const int sel_row=sel/g.cols;if(sel_row<top_row)top_row=sel_row;if(sel_row>=top_row+g.rows)top_row=sel_row-g.rows+1;const int last_row=n>0?(n-1)/g.cols:0;top_row=std::max(0,std::min(top_row,last_row-g.rows+1));top=top_row*g.cols;return;}
 const int vis=guide_rows_visible();if(sel<top)top=sel;if(sel>=top+vis)top=sel-vis+1;top=std::max(0,std::min(top,n-vis));
}
const JFProgram* focus_cell(int row){if(row<0||row>=n)return nullptr;const JFProgram* ps[24];int k=progs_of(ch[row].id,ps,24);const JFProgram* best=nullptr;for(int i=0;i<k;++i)if(ps[i]->start_ticks<=gt&&gt<ps[i]->end_ticks)best=ps[i];return best;}
void guide_move_time(int dir){
 const JFProgram* cur=focus_cell(sel);const JFProgram* ps[24];const int k=progs_of(ch[sel].id,ps,24);bool moved=false;
 if(dir>0){const uint64_t after=cur?cur->end_ticks:gt;for(int i=0;i<k&&!moved;++i)if(ps[i]->start_ticks>=after){gt=ps[i]->start_ticks;moved=true;}if(!moved)gt=after+(cur?0:T_30M);}
 else{const uint64_t before=cur?cur->start_ticks:gt;for(int i=k-1;i>=0&&!moved;--i)if(ps[i]->end_ticks<=before){gt=ps[i]->start_ticks;moved=true;}if(!moved)gt=before>T_30M?before-T_30M:before;}
 // Scroll the window so the focus stays inside it, never before the current half hour.
 if(gt>=win0+T_3H)win0=floor_half_hour(gt)-T_30M*2;if(gt<win0)win0=floor_half_hour(gt);if(win0<base_win0)win0=base_win0;
}
bool at_first_column(){if(win0>base_win0)return false;const JFProgram* ps[24];const int k=progs_of(ch[sel].id,ps,24);const JFProgram* cur=focus_cell(sel);if(!cur)return gt<=win0+T_30M;for(int i=0;i<k;++i)if(ps[i]->end_ticks<=cur->start_ticks&&ps[i]->end_ticks>win0)return false;return true;}
void open_guide(){const uint64_t now=jf_now_ticks();guide=true;base_win0=floor_half_hour(now);win0=base_win0;gt=now;keep_visible();view_changed=now_ms();}
void close_guide(){guide=false;keep_visible();view_changed=now_ms();}

// ---- drawing ----
uint32_t mixc(uint32_t a,uint32_t b,int t){int r=(((a>>16)&255)*(255-t)+((b>>16)&255)*t)/255,g=(((a>>8)&255)*(255-t)+((b>>8)&255)*t)/255,bl=((a&255)*(255-t)+(b&255)*t)/255;return (r<<16)|(g<<8)|bl;}
std::string fit(const std::string& v,int w,int size){if(face_text_width(v,size,TextFace::Classic)<=w)return v;std::string s=v;while(!s.empty()&&face_text_width(s+"...",size,TextFace::Classic)>w){size_t e=s.size()-1;while(e&&((unsigned char)s[e]&0xc0)==0x80)--e;s.resize(e);}return s+"...";}
void clip(int x,int y,const std::string& t,int size,uint32_t c,int w){if(w>8)face_text(x,y,fit(t,w,size),c,size,TextFace::Classic);}
void border(int x,int y,int w,int h,uint32_t c,int t=1){rect(x,y,w,t,c);rect(x,y+h-t,w,t,c);rect(x,y,t,h,c);rect(x+w-t,y,t,h,c);}
std::string logo_key(const JFChannel& c){return std::string("channel:")+c.id;}
void draw_logo(const JFChannel& c,int x,int y,int w,int h){
 if(c.logo_tag[0]){load_image(logo_key(c),"/Items/"+encode(c.id)+"/Images/Primary?MaxHeight=160&Quality=90");if(draw_art(logo_key(c),x,y,w,h))return;}
 // No logo: the channel's initials, as a monogram.
 std::string mono;for(const char* p=c.name;*p&&mono.size()<3;++p)if((*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9'))mono+=*p;if(mono.empty()&&c.name[0])mono=std::string(1,c.name[0]);
 const int size=std::max(14,h/3);face_text(x+(w-face_text_width(mono,size,TextFace::Classic))/2,y+(h-size)/2,mono,g_theme.text_faint,size,TextFace::Classic);
}
void draw_cat_tabs(int x,int y,int right){
 const int size=16,gap=34,lw=draw_button_icon('L',-200,-200,22),rw=lw;const int tabs_x=x+lw+18,avail=right-rw-18-tabs_x;
 std::vector<std::string> lab(npages);std::vector<int> w(npages);for(int p=0;p<npages;++p){lab[p]=key_label(pages[p].key,pages[p].cat);w[p]=face_text_width(lab[p],size,TextFace::Classic);}
 int first=0;for(;;){int span=0;for(int p=first;p<=page&&p<npages;++p)span+=w[p]+(p>first?gap:0);if(span<=avail||first>=page)break;++first;}
 draw_button_icon('L',x,y-3,22);int tx=tabs_x;
 for(int p=first;p<npages;++p){if(tx+w[p]>tabs_x+avail)break;const bool on=p==page;face_text(tx,y,lab[p],on?g_theme.white:g_theme.text_faint,size,TextFace::Classic);if(on)rect(tx,y+24,w[p],3,g_theme.accent);tx+=w[p]+gap;}
 draw_button_icon('R',tx-gap+18,y-3,22);
}
void draw_screen(int header_focus){
 app_header("","Home",header_focus);const LiveGrid g=live_grid();const int x=guide?GUIDE_X0:g.x0;
 face_text(x,86,"Live TV",g_theme.text,24,TextFace::Classic);
 if(search_on)clip(x,122,"Search: "+term+"  ("+std::to_string(n)+" found)",15,g_theme.accent_alt,WIDTH-2*x);
 else if(paging()&&!guide)draw_cat_tabs(x,122,WIDTH-x);
 if(!guide&&n>0){int last=std::min(top+g.cols*g.rows,n);std::string pos=std::to_string(top+1)+" - "+std::to_string(last)+" / "+std::to_string(n);face_text(WIDTH-g.x0-face_text_width(pos,14,TextFace::Classic),95,pos,g_theme.text_dim,14,TextFace::Classic);}
 const uint64_t now=jf_now_ticks();
 if(n<=0){std::string m=failed?"Couldn't load the channel list":!loaded?"Loading channels...":search_on?"No channels found":cur_key()==LP_FAVS?"No favourites yet. Press Triangle on a channel.":cur_key()==LP_RECENT?"Nothing watched yet":"No channels";face_text((WIDTH-face_text_width(m,18,TextFace::Classic))/2,BAND_TOP+60,m,g_theme.text_dim,18,TextFace::Classic);}
 else if(!guide){
  for(int rel=0;rel<g.cols*g.rows&&top+rel<n;++rel){
   const int i=top+rel;const JFChannel& c=ch[i];const int cx=card_x(g,rel),cy=card_y(g,rel);const bool on=header_focus<0&&i==sel;
   rect(cx,cy,g.card_w,g.card_h,on?g_theme.panel_hi:0x0A0C12);
   const int ls=g.card_h*4/5;draw_logo(c,cx+(g.card_w-ls)/2,cy+(g.card_h-ls)/2,ls,ls);
   const NowNext nn=now_next(c,now);const int pm=livetv_progress_permille(now,nn.now_start,nn.now_end);
   if(pm>=0){const int bx=cx+10,bw=g.card_w-20,by=cy+g.card_h-10;rect(bx,by,bw,4,g_theme.hairline);rect(bx,by,bw*pm/1000,4,g_theme.accent);}
   if(c.favourite)draw_star(cx+10,cy+8,16,g_theme.accent_alt);
   if(on)border(cx-3,cy-3,g.card_w+6,g.card_h+6,g_theme.focus_ring,2);
   const int ty=cy+g.card_h+8;int nx=cx;
   if(c.number[0]){face_text(cx,ty,c.number,g_theme.text_faint,16,TextFace::Classic);nx+=face_text_width(c.number,16,TextFace::Classic)+10;}
   clip(nx,ty,c.name,16,on?g_theme.white:g_theme.text,cx+g.card_w-nx);
   if(!nn.now.empty())clip(cx,ty+22,nn.now,13,g_theme.text_dim,g.card_w);
  }
 }
 else{
  // The guide: pinned channels, three hours of programmes, the focus cell.
  for(int k=0;k<6;++k){char hm[16];fmt_hm(win0+(uint64_t)k*T_30M,hm,sizeof hm);const int gx=guide_cells_x()+guide_cells_w()*k/6;face_text(gx+4,BAND_TOP+2,hm,g_theme.text_dim,13,TextFace::Classic);rect(gx,BAND_TOP+18,1,10,g_theme.hairline);}
  if(now>=win0&&now<win0+T_3H){const int nx=x_of_time(now);rect(nx,guide_top()-4,2,BAND_BOTTOM-guide_top(),g_theme.accent_alt);}
  for(int r=0;r<guide_rows_visible()&&top+r<n;++r){
   const int i=top+r,y=guide_top()+r*GUIDE_PITCH;const JFChannel& c=ch[i];const bool on=i==sel;
   rect(GUIDE_X0,y,GUIDE_LOGO_W,GUIDE_ROW_H,on?g_theme.panel_hi:g_theme.panel);if(on)rect(GUIDE_X0-4,y,3,GUIDE_ROW_H,g_theme.accent);
   draw_logo(c,GUIDE_X0+GUIDE_LOGO_W-78,y+(GUIDE_ROW_H-40)/2,70,40);
   if(c.number[0])face_text(GUIDE_X0+10,y+8,c.number,on?g_theme.white:g_theme.text_faint,15,TextFace::Classic);
   clip(GUIDE_X0+10,y+30,c.name,12,on?g_theme.text:g_theme.text_dim,66);
   const JFProgram* ps[24];const int k=progs_of(c.id,ps,24);const JFProgram* foc=on?focus_cell(i):nullptr;
   for(int p=0;p<k;++p){if(ps[p]->end_ticks<=win0||ps[p]->start_ticks>=win0+T_3H)continue;const int x0=x_of_time(ps[p]->start_ticks),x1=x_of_time(ps[p]->end_ticks);if(x1-x0<8)continue;const bool f=foc==ps[p];
    rect(x0+1,y,x1-x0-2,GUIDE_ROW_H,f?g_theme.panel_hi:g_theme.panel);if(f)border(x0+1,y,x1-x0-2,GUIDE_ROW_H,g_theme.focus_ring,2);
    if(x1-x0>=40)clip(x0+10,y+(GUIDE_ROW_H-14)/2-1,ps[p]->name,14,f?g_theme.white:g_theme.text,x1-x0-20);}
   if(!k)face_text(guide_cells_x()+10,y+(GUIDE_ROW_H-14)/2-1,"No guide information",g_theme.text_faint,14,TextFace::Classic);
  }
 }
 if(header_focus>=0)hint_bar({{'D',"Nav"},{'X',"Select"},{'C',"Back"}});
 else if(guide)hint_bar({{'X',"Watch / details"},{'D',"Move"},{'C',"List"}});
 else if(search_on)hint_bar({{'X',"Watch"},{'T',"Favourite"},{'S',"Guide"},{'P',"Search"},{'C',"Back"}});
 else if(paging())hint_bar({{'X',"Watch"},{'T',"Favourite"},{'S',"Guide"},{'L',""},{'R',"Category"},{'P',"Search"},{'C',"Back"}});
 else hint_bar({{'X',"Watch"},{'T',"Favourite"},{'S',"Guide"},{'P',"Search"},{'C',"Back"}});
}
void show_details(const JFChannel& c,const JFProgram& p){
 char overview[1024]="";jf_fetch_program_overview(p.id,overview,sizeof overview);
 UiInputScope scope;
 for(;;){
  app_header("","Home");const int pw=640,ph=330,px=(WIDTH-pw)/2,py=(HEIGHT-ph)/2;rect(px,py,pw,ph,g_theme.panel);border(px,py,pw,ph,g_theme.hairline);
  const int cx=px+30,mw=pw-60;int y=py+26;clip(cx,y,p.name,24,g_theme.white,mw);y+=40;
  char a[16],b[16];fmt_hm(p.start_ticks,a,sizeof a);fmt_hm(p.end_ticks,b,sizeof b);clip(cx,y,std::string(a)+" - "+b+"  \xC2\xB7  "+c.name,15,g_theme.accent_alt,mw);y+=28;
  if(p.episode_title[0]){clip(cx,y,p.episode_title,15,g_theme.text,mw);y+=28;}
  if(overview[0])paragraph(cx,y,overview,mw,6,g_theme.text_dim,15,22);else face_text(cx,y,"No description.",g_theme.text_faint,15,TextFace::Classic);
  hint_bar({{'X',"OK"}});present();auto bt=ui_input();if(bt&(ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_CIRCLE))return;
 }
}
void play_channel(int idx,const std::function<void(const Item&)>& play){
 if(idx<0||idx>=n)return;Item item;snprintf(item.base.id,sizeof item.base.id,"%s",ch[idx].id);snprintf(item.base.name,sizeof item.base.name,"%s",ch[idx].name);snprintf(item.base.type,sizeof item.base.type,"TvChannel");
 note_recent(item.base.id);play(item);last_refresh=0;
}

}   // namespace

int livetv_screen(const std::function<void(const Item&)>& play){
 if(!ensure_worker()){last_error="Live TV is unavailable";return -2;}
 utc_offset=jf_utc_offset_secs();guide=false;top=0;if(search_on){search_on=false;term.clear();list_changed();}
 if(!index_ok)index_tried=false;last_refresh=0;view_changed=now_ms();if(sel>=n)sel=0;keep_visible();
 UiInputScope scope;int header=-1;
 for(;;){
  tick();draw_screen(header);present();const uint32_t b=ui_input();
  if(header>=0){
   if(b&ORBIS_PAD_BUTTON_LEFT)header=(header+2)%3;else if(b&ORBIS_PAD_BUTTON_RIGHT)header=(header+1)%3;
   else if(b&(ORBIS_PAD_BUTTON_DOWN|ORBIS_PAD_BUTTON_CIRCLE))header=-1;else if(b&ORBIS_PAD_BUTTON_CROSS)return header;
   continue;
  }
  if(b&ORBIS_PAD_BUTTON_L1)return 0;if(b&ORBIS_PAD_BUTTON_R1)return 1;if(b&ORBIS_PAD_BUTTON_OPTIONS)return 2;
  if(!guide){
   if(b&ORBIS_PAD_BUTTON_TOUCH_PAD){std::string query=term;if(keyboard("Search channels",query)){while(!query.empty()&&query.back()==' ')query.pop_back();if(!query.empty()){term=query;search_on=true;list_changed();}}ui_input_sync();continue;}
   if(b&ORBIS_PAD_BUTTON_CIRCLE){if(search_on){search_on=false;term.clear();list_changed();continue;}return -1;}
   if(paging()&&!search_on){if(b&ORBIS_PAD_BUTTON_L2){set_page(page-1);continue;}if(b&ORBIS_PAD_BUTTON_R2){set_page(page+1);continue;}}
   if(n<=0){if(b&ORBIS_PAD_BUTTON_UP)header=0;continue;}
   const int cols=live_grid().cols,was=sel;
   if(b&ORBIS_PAD_BUTTON_LEFT&&sel%cols>0)sel--;
   if(b&ORBIS_PAD_BUTTON_RIGHT&&sel%cols<cols-1&&sel<n-1)sel++;
   if(b&ORBIS_PAD_BUTTON_UP){if(sel>=cols)sel-=cols;else header=0;}
   if(b&ORBIS_PAD_BUTTON_DOWN&&sel/cols<(n-1)/cols)sel=sel+cols<n?sel+cols:n-1;
   if(sel!=was)keep_visible();
   if(b&ORBIS_PAD_BUTTON_CROSS){play_channel(sel,play);ui_input_sync();continue;}
   if(b&ORBIS_PAD_BUTTON_TRIANGLE){
    // Star it now; the list re-sorts and the selection follows the channel.
    const bool want=!ch[sel].favourite;const std::string id=ch[sel].id;
    if(jf_set_favourite(id.c_str(),want)){
     if(!want&&cur_key()==LP_FAVS){std::copy(ch.begin()+sel+1,ch.begin()+n,ch.begin()+sel);n--;if(sel>=n)sel=n>0?n-1:0;}
     else{ch[sel].favourite=want;livetv_sort_channels(ch.data(),n);for(int i=0;i<n;++i)if(id==ch[i].id)sel=i;}
     pg_rows=0;keep_visible();}
   }
   if(b&ORBIS_PAD_BUTTON_SQUARE&&have_guide())open_guide();
   continue;
  }
  // The guide.
  if(b&ORBIS_PAD_BUTTON_CIRCLE){close_guide();continue;}
  const int was=sel;if(b&ORBIS_PAD_BUTTON_UP&&sel>0)sel--;if(b&ORBIS_PAD_BUTTON_DOWN&&sel<n-1)sel++;if(sel!=was)keep_visible();
  if(b&ORBIS_PAD_BUTTON_LEFT){if(at_first_column()){close_guide();continue;}guide_move_time(-1);}
  if(b&ORBIS_PAD_BUTTON_RIGHT)guide_move_time(+1);
  if(b&ORBIS_PAD_BUTTON_CROSS){
   const uint64_t now=jf_now_ticks();const JFProgram* p=focus_cell(sel);
   if(p&&!(p->start_ticks<=now&&now<p->end_ticks)){const JFProgram copy=*p;const JFChannel c=ch[sel];show_details(c,copy);}
   else play_channel(sel,play);
   ui_input_sync();
  }
 }
}
void livetv_stop(){
 pthread_mutex_lock(&lock);bool join=running;stopping=true;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&lock);
 if(join)pthread_join(thread,nullptr);running=false;stopping=false;jstate=J_IDLE;index_ok=false;index_tried=false;n=0;loaded=false;
}
#ifdef HOST_TEST
// Host previews: draw the page with fixture channels.
void livetv_preview(const std::vector<JFChannel>& channels,int selected,bool show_guide){ch=channels;ch.resize(std::max<size_t>(channels.size(),1));n=(int)channels.size();loaded=true;sel=selected;guide=show_guide;if(guide){base_win0=win0=floor_half_hour(jf_now_ticks());gt=jf_now_ticks();}keep_visible();draw_screen(-1);}
#endif
