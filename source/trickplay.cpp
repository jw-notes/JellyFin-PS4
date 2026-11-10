// JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
// GPL-3.0-or-later, with the attribution terms in NOTICE.
//
// Seek preview thumbnails, from wherever the film comes from.
//
// The server's trickplay (Jellyfin 10.9+) comes first: JPEG sheets of tile_w x
// tile_h thumbnails (10 x 10 of 320 x 180 by default: 3200 x 1800, some 17 MB
// once decoded). A sheet is fetched when a thumbnail on it is first asked for,
// cut into thumbnails at the size they are shown, and let go.
//
// Without it (a USB drive, a download, a server that has made none) the frame
// is taken from the file itself: FFmpeg opens it beside the player, seeks to
// the keyframe before the scrub target and decodes that one frame, keyframes
// only. Only the newest target is worked on, and the file is closed again once
// the scrubbing stops, so it holds no second connection while the film plays.
//
// Either way only the small thumbnails are kept, at most MAX_THUMBS of them,
// those nearest to where the scrub is.
#include "trickplay.h"
#include "platform.h"
#include "thread_priority.h"
#include "local_media.h"
#include "mono_clock.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include "stb_image.h"
#ifndef HOST_TEST
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#endif

static constexpr int THUMB_W=224;        // on screen; the height follows the picture's shape
static constexpr size_t MAX_THUMBS=120;  // ~13 MB at 224 x 126
static constexpr int SHEET_BYTES=8*1024*1024;
static constexpr uint64_t FILE_STEP_MS=10000;  // a frame from the file every 10 s of the film
static constexpr uint64_t FILE_IDLE_MS=8000;   // closed again after this long without a request

enum Mode { MODE_NONE, MODE_SHEETS, MODE_FILE };
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static bool worker_running=false;
static Mode mode=MODE_NONE;
static TrickplayInfo info;
static std::string file_url;
static unsigned generation=0;            // a new film: work for the old one is dropped
static int thumb_h=0,last_index=0;
static uint64_t last_request_ms=0;
static std::map<int,std::vector<uint32_t>> thumbs;
static std::set<int> done;               // sheets (or file frames) fetched or failed: not asked for again
static std::deque<int> wanted;           // sheets, or file frames, most wanted first

static int per_sheet(){return std::max(1,info.tile_w*info.tile_h);}
static void keep_nearest(){
 while(thumbs.size()>MAX_THUMBS){
  auto far=thumbs.begin();for(auto i=thumbs.begin();i!=thumbs.end();++i)if(abs(i->first-last_index)>abs(far->first-last_index))far=i;
  const int gone=far->first;thumbs.erase(far);done.erase(mode==MODE_SHEETS?gone/per_sheet():gone);
 }
}

// ---- the server's sheets --------------------------------------------------------------
// One thumbnail, cell (cx, cy) of size cw x ch in an RGB sheet, box-filtered to THUMB_W x th.
static std::vector<uint32_t> cut(const uint8_t* rgb,int sw,int sh,int cx,int cy,int cw,int ch,int th){
 std::vector<uint32_t> out((size_t)THUMB_W*th,0xFF000000);
 for(int y=0;y<th;++y){
  const int y0=cy+y*ch/th,y1=std::max(y0+1,cy+(y+1)*ch/th);
  for(int x=0;x<THUMB_W;++x){
   const int x0=cx+x*cw/THUMB_W,x1=std::max(x0+1,cx+(x+1)*cw/THUMB_W);
   unsigned r=0,g=0,b=0,n=0;
   for(int sy=y0;sy<y1&&sy<sh;++sy)for(int sx=x0;sx<x1&&sx<sw;++sx){const uint8_t* p=rgb+((size_t)sy*sw+sx)*3;r+=p[0];g+=p[1];b+=p[2];++n;}
   if(n)out[(size_t)y*THUMB_W+x]=0xFF000000u|((r/n)<<16)|((g/n)<<8)|(b/n);
  }
 }
 return out;
}
static std::map<int,std::vector<uint32_t>> fetch_sheet(const TrickplayInfo& job,int sheet,int th,std::vector<uint8_t>& buffer){
 std::map<int,std::vector<uint32_t>> cells;
 const std::string url=std::string(g_server)+"/Videos/"+encode(job.item)+"/Trickplay/"+std::to_string(job.width)+"/"+std::to_string(sheet)+".jpg?MediaSourceId="+encode(job.source);
 const int size=http_fetch_binary(url.c_str(),g_token,buffer.data(),SHEET_BYTES);
 if(size<=0||size>=SHEET_BYTES)return cells;
 int sw=0,sh=0,channels=0;uint8_t* rgb=stbi_load_from_memory(buffer.data(),size,&sw,&sh,&channels,3);if(!rgb)return cells;
 // A sheet is tile_w thumbnails across; the last one may have fewer rows.
 const int per=job.tile_w*job.tile_h;
 for(int k=0;k<per;++k){const int index=sheet*per+k;if(index>=job.count)break;
  const int cx=(k%job.tile_w)*job.width,cy=(k/job.tile_w)*job.height;if(cx+job.width>sw||cy+job.height>sh)break;
  cells[index]=cut(rgb,sw,sh,cx,cy,job.width,job.height,th);}
 stbi_image_free(rgb);
 return cells;
}

// ---- frames from the file ------------------------------------------------------------
#ifndef HOST_TEST
struct FileFrames {
 std::string url;
 AVFormatContext* fmt=nullptr;AVCodecContext* dec=nullptr;SwsContext* sws=nullptr;AVIOContext* avio=nullptr;void* ftp=nullptr;
 AVPacket* packet=nullptr;AVFrame* frame=nullptr;int stream=-1;bool failed=false;
};
static int ftp_read(void* opaque,uint8_t* buf,int size){const int64_t n=ftp_reader_read(opaque,(char*)buf,(uint64_t)size);return n>0?(int)n:n==0?AVERROR_EOF:AVERROR(EIO);}
static int64_t ftp_seek(void* opaque,int64_t offset,int whence){
 if(whence==AVSEEK_SIZE)return ftp_reader_size(opaque);
 if((whence&~AVSEEK_FORCE)!=SEEK_SET)return -1;return ftp_reader_seek(opaque,(uint64_t)offset);}
static void file_close(FileFrames& f){
 if(f.sws)sws_freeContext(f.sws);if(f.frame)av_frame_free(&f.frame);if(f.packet)av_packet_free(&f.packet);
 if(f.dec)avcodec_free_context(&f.dec);
 if(f.fmt)avformat_close_input(&f.fmt);   // frees the format context, not a custom AVIO's buffer
 if(f.avio){av_freep(&f.avio->buffer);avio_context_free(&f.avio);}
 if(f.ftp)ftp_reader_close(f.ftp);
 const std::string url=f.url;const bool failed=f.failed;f=FileFrames();f.url=url;f.failed=failed;
}
static bool file_open(FileFrames& f){
 if(f.fmt)return true;if(f.failed)return false;
 f.fmt=avformat_alloc_context();if(!f.fmt){f.failed=true;return false;}
 AVDictionary* opts=nullptr;
 std::string open_url=f.url;
 if(local_is_drive_url(f.url)){
  // On a console that kept its sandbox: GoldHEN's FTP server, as the player reads it.
  f.ftp=ftp_reader_open(local_url_path(f.url));
  uint8_t* buf=(uint8_t*)av_malloc(64*1024);
  if(!f.ftp||!buf){av_free(buf);file_close(f);f.failed=true;plog("Seek preview: the file on the USB drive could not be opened");return false;}
  f.avio=avio_alloc_context(buf,64*1024,0,f.ftp,ftp_read,nullptr,ftp_seek);f.fmt->pb=f.avio;f.fmt->flags|=AVFMT_FLAG_CUSTOM_IO;open_url="";
 }else av_dict_set(&opts,"rw_timeout","8000000",0);
 av_dict_set(&opts,"probesize","2000000",0);av_dict_set(&opts,"analyzeduration","1000000",0);
 const int rc=avformat_open_input(&f.fmt,open_url.c_str(),nullptr,&opts);av_dict_free(&opts);
 if(rc<0){f.fmt=nullptr;file_close(f);f.failed=true;plog("Seek preview: the file could not be opened");return false;}
 avformat_find_stream_info(f.fmt,nullptr);
 const AVCodec* codec=nullptr;f.stream=av_find_best_stream(f.fmt,AVMEDIA_TYPE_VIDEO,-1,-1,&codec,0);
 if(f.stream<0||!codec){file_close(f);f.failed=true;plog("Seek preview: no video in the file");return false;}
 f.dec=avcodec_alloc_context3(codec);avcodec_parameters_to_context(f.dec,f.fmt->streams[f.stream]->codecpar);
 // Keyframes only: one frame each time, decoded as cheaply as it can be.
 f.dec->skip_frame=AVDISCARD_NONKEY;f.dec->thread_count=1;
 if(avcodec_open2(f.dec,codec,nullptr)<0){file_close(f);f.failed=true;plog("Seek preview: the video could not be decoded");return false;}
 f.packet=av_packet_alloc();f.frame=av_frame_alloc();
 plog(("Seek preview: frames from the file ("+std::string(codec->name)+")").c_str());
 return true;
}
// The keyframe at or before `ms`, scaled to THUMB_W wide.
static bool file_frame(FileFrames& f,uint64_t ms,std::vector<uint32_t>& out,int& th){
 if(!file_open(f))return false;
 AVStream* st=f.fmt->streams[f.stream];
 int64_t ts=av_rescale_q((int64_t)ms,AVRational{1,1000},st->time_base);if(st->start_time!=AV_NOPTS_VALUE)ts+=st->start_time;
 if(av_seek_frame(f.fmt,f.stream,ts,AVSEEK_FLAG_BACKWARD)<0)return false;
 avcodec_flush_buffers(f.dec);
 bool got=false;
 for(int packets=0;!got&&packets<400&&av_read_frame(f.fmt,f.packet)>=0;++packets){
  if(f.packet->stream_index==f.stream&&avcodec_send_packet(f.dec,f.packet)>=0)got=avcodec_receive_frame(f.dec,f.frame)>=0;
  av_packet_unref(f.packet);}
 if(!got){avcodec_send_packet(f.dec,nullptr);got=avcodec_receive_frame(f.dec,f.frame)>=0;}
 if(!got||f.frame->width<=0||f.frame->height<=0)return false;
 // The picture's shape, with its pixels' own shape (anamorphic DVDs and the like).
 double aspect=(double)f.frame->width/f.frame->height;const AVRational sar=f.frame->sample_aspect_ratio;if(sar.num>0&&sar.den>0)aspect*=(double)sar.num/sar.den;
 th=std::max(2,std::min(THUMB_W,(int)(THUMB_W/aspect+0.5)))&~1;
 f.sws=sws_getCachedContext(f.sws,f.frame->width,f.frame->height,(AVPixelFormat)f.frame->format,THUMB_W,th,AV_PIX_FMT_BGRA,SWS_BILINEAR,nullptr,nullptr,nullptr);
 if(!f.sws)return false;
 out.assign((size_t)THUMB_W*th,0);uint8_t* dst[4]={(uint8_t*)out.data(),nullptr,nullptr,nullptr};int pitch[4]={THUMB_W*4,0,0,0};
 sws_scale(f.sws,f.frame->data,f.frame->linesize,0,f.frame->height,dst,pitch);
 for(auto& p:out)p|=0xFF000000u;   // BGRA in memory is 0xAARRGGBB here
 av_frame_unref(f.frame);
 return true;
}
#endif

// ---- the worker -------------------------------------------------------------------------
static void* worker(void*){
 background_priority();
 std::vector<uint8_t> buffer;
#ifndef HOST_TEST
 FileFrames file;unsigned file_gen=0;
#endif
 for(;;){
  pthread_mutex_lock(&lock);
#ifndef HOST_TEST
  // A file left open: closed once the scrubbing has stopped, or the film changed.
  while(wanted.empty()){
   if(file.fmt&&(file_gen!=generation||mono_ms()-last_request_ms>=FILE_IDLE_MS)){pthread_mutex_unlock(&lock);file_close(file);plog("Seek preview: file closed");pthread_mutex_lock(&lock);continue;}
   if(file.fmt){struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_sec+=1;pthread_cond_timedwait(&wake,&lock,&until);}
   else pthread_cond_wait(&wake,&lock);
  }
#else
  while(wanted.empty())pthread_cond_wait(&wake,&lock);
#endif
  const int job_index=wanted.front();wanted.pop_front();
  const Mode job_mode=mode;const TrickplayInfo job=info;const std::string url=file_url;const unsigned gen=generation;const int th=thumb_h;
  pthread_mutex_unlock(&lock);

  std::map<int,std::vector<uint32_t>> cells;int got_h=th;
  if(job_mode==MODE_SHEETS){if(buffer.empty())buffer.resize(SHEET_BYTES);cells=fetch_sheet(job,job_index,th,buffer);}
#ifndef HOST_TEST
  else if(job_mode==MODE_FILE){
   if(file_gen!=gen||file.url!=url){file_close(file);file=FileFrames();file.url=url;file_gen=gen;}
   std::vector<uint32_t> px;if(file_frame(file,(uint64_t)job_index*FILE_STEP_MS,px,got_h))cells[job_index]=std::move(px);
  }
#endif
  pthread_mutex_lock(&lock);
  if(gen==generation){
   done.insert(job_index);
   if(job_mode==MODE_FILE&&!cells.empty())thumb_h=got_h;
   for(auto& c:cells)thumbs[c.first]=std::move(c.second);
   keep_nearest();
   if(job_mode==MODE_SHEETS)plog(("Trickplay: sheet "+std::to_string(job_index)+(cells.empty()?" could not be read":", "+std::to_string(cells.size())+" thumbnails")).c_str());
  }
  pthread_mutex_unlock(&lock);
 }
 return nullptr;
}
static void start_worker(){if(!worker_running){pthread_t t;if(pthread_create(&t,nullptr,worker,nullptr)==0){pthread_detach(t);worker_running=true;}}}

void trickplay_begin(const TrickplayInfo& next){
 pthread_mutex_lock(&lock);
 ++generation;info=next;file_url.clear();thumbs.clear();done.clear();wanted.clear();last_index=0;
 mode=info.count?MODE_SHEETS:MODE_NONE;
 thumb_h=info.width>0?std::max(1,THUMB_W*info.height/info.width):0;
 if(mode!=MODE_NONE)start_worker();
 pthread_cond_signal(&wake);
 pthread_mutex_unlock(&lock);
 if(next.count)plog(("Trickplay: "+std::to_string(next.count)+" thumbnails "+std::to_string(next.width)+"x"+std::to_string(next.height)+", one every "+std::to_string(next.interval_ms/1000)+" s").c_str());
}
void trickplay_begin_file(const std::string& url){
#ifdef HOST_TEST
 (void)url;trickplay_end();
#else
 pthread_mutex_lock(&lock);
 ++generation;info=TrickplayInfo();file_url=url;thumbs.clear();done.clear();wanted.clear();last_index=0;
 mode=url.empty()?MODE_NONE:MODE_FILE;thumb_h=THUMB_W*9/16;
 if(mode!=MODE_NONE)start_worker();
 pthread_cond_signal(&wake);
 pthread_mutex_unlock(&lock);
#endif
}
void trickplay_end(){trickplay_begin(TrickplayInfo());}
bool trickplay_available(){pthread_mutex_lock(&lock);const bool yes=mode!=MODE_NONE&&worker_running;pthread_mutex_unlock(&lock);return yes;}

#ifdef HOST_TEST
static std::vector<uint32_t> preview;static int preview_w=0,preview_h=0;
void trickplay_preview(const std::vector<uint32_t>& px,int w,int h){preview=px;preview_w=w;preview_h=h;}
#endif
bool trickplay_thumb(uint64_t ms,std::vector<uint32_t>& px,int& w,int& h){
#ifdef HOST_TEST
 if(!preview.empty()){px=preview;w=preview_w;h=preview_h;return true;}
#endif
 pthread_mutex_lock(&lock);
 if(mode==MODE_NONE||!worker_running){pthread_mutex_unlock(&lock);return false;}
 last_request_ms=mono_ms();
 int index=0,reach=0;
 if(mode==MODE_SHEETS){index=(int)std::min<uint64_t>((uint64_t)info.count-1,ms/info.interval_ms);reach=per_sheet();}
 else{index=(int)(ms/FILE_STEP_MS);reach=6;}   // a file: show one from up to a minute away meanwhile
 last_index=index;
 auto want=[&](int job){if(job<0||done.count(job))return;
  if(std::find(wanted.begin(),wanted.end(),job)!=wanted.end())return;
  wanted.push_front(job);
  // A file has only the newest target worked on; sheets keep a short line.
  while(wanted.size()>(mode==MODE_FILE?1u:3u))wanted.pop_back();
  pthread_cond_signal(&wake);};
 auto found=thumbs.find(index);
 if(found==thumbs.end()){
  if(mode==MODE_SHEETS)want(index/per_sheet());else want(index);
  // Meanwhile the nearest one here, if it is close.
  for(int d=1;d<reach&&found==thumbs.end();++d){found=thumbs.find(index-d);if(found==thumbs.end())found=thumbs.find(index+d);}
 }else if(mode==MODE_SHEETS){
  // Near a sheet's edge: the next one in either direction, ahead of the scrub.
  if(mode==MODE_SHEETS&&index/per_sheet()*per_sheet()+per_sheet()*3/4<=index)want(index/per_sheet()+1);
  else if(index%per_sheet()<per_sheet()/4)want(index/per_sheet()-1);
 }
 const bool ok=found!=thumbs.end();
 if(ok){px=found->second;w=THUMB_W;h=(int)(found->second.size()/THUMB_W);}
 pthread_mutex_unlock(&lock);
 return ok;
}
