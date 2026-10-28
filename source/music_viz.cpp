#include "music_viz.h"
#include "mono_clock.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <atomic>

// The PS3's numbers (music_fft.cpp): 2048 points keep 23 Hz bins, so the
// bass bands do not collapse onto one bin.
constexpr int FFT_N=2048,FFT_BITS=11,FFT_HALF=FFT_N/2,TAP_CAP=4096;
constexpr float VIZ_RATE=48000.0f,BAND_LO=40.0f,BAND_HI=16000.0f;
// SDL hands MPV buffers of tens of milliseconds, slower than the UI's frames:
// a frame without new samples reuses the last analysis until this much time
// has passed with none (pause, a stall), then the bars fall.
constexpr uint64_t STALE_US=150000;

static float tap[TAP_CAP];
static int tap_wr=0;
static uint64_t tap_count=0,tap_at_us=0;
static pthread_mutex_t tap_lock=PTHREAD_MUTEX_INITIALIZER;

static float hann[FFT_N],tw_re[FFT_HALF],tw_im[FFT_HALF];
static uint16_t brev[FFT_N];
static int band_lo[MUSIC_VIZ_BANDS],band_hi[MUSIC_VIZ_BANDS];
static float band_ctr[MUSIC_VIZ_BANDS];
static bool tables=false;
static float smooth[MUSIC_VIZ_BANDS],last_raw[MUSIC_VIZ_BANDS];

static uint64_t now_us(){return mono_us();}
static void build_tables(){
 if(tables)return;
 for(int i=0;i<FFT_N;++i)hann[i]=0.5f-0.5f*cosf(2.0f*3.14159265f*i/(FFT_N-1));
 for(int k=0;k<FFT_HALF;++k){float a=-2.0f*3.14159265f*k/FFT_N;tw_re[k]=cosf(a);tw_im[k]=sinf(a);}
 for(int i=0;i<FFT_N;++i){unsigned r=0;for(int b=0;b<FFT_BITS;++b)if(i&(1u<<b))r|=1u<<(FFT_BITS-1-b);brev[i]=(uint16_t)r;}
 const float binw=VIZ_RATE/FFT_N;
 for(int i=0;i<MUSIC_VIZ_BANDS;++i){
  float fl=BAND_LO*powf(BAND_HI/BAND_LO,(float)i/MUSIC_VIZ_BANDS),fh=BAND_LO*powf(BAND_HI/BAND_LO,(float)(i+1)/MUSIC_VIZ_BANDS);
  int lo=(int)(fl/binw),hi=(int)(fh/binw);if(lo<1)lo=1;if(hi<=lo)hi=lo+1;if(hi>FFT_HALF)hi=FFT_HALF;if(lo>=hi)lo=hi-1;
  band_lo[i]=lo;band_hi[i]=hi;float ctr=sqrtf(fl*fh)/binw;band_ctr[i]=ctr<1?1:ctr>FFT_HALF-2.0f?FFT_HALF-2.0f:ctr;}
 tables=true;
}
void music_viz_reset(){
 build_tables();
 pthread_mutex_lock(&tap_lock);memset(tap,0,sizeof tap);tap_wr=0;tap_count=0;tap_at_us=0;pthread_mutex_unlock(&tap_lock);
 memset(smooth,0,sizeof smooth);memset(last_raw,0,sizeof last_raw);
}
void music_viz_push(const float* lr,int n_pairs){
 if(n_pairs<=0)return;
 pthread_mutex_lock(&tap_lock);
 for(int i=0;i<n_pairs;++i){tap[tap_wr]=0.5f*(lr[i*2]+lr[i*2+1]);tap_wr=(tap_wr+1)&(TAP_CAP-1);}
 tap_count+=(uint64_t)n_pairs;tap_at_us=now_us();
 pthread_mutex_unlock(&tap_lock);
}
static void fft(float* re,float* im){
 for(int len=2;len<=FFT_N;len<<=1){const int half=len>>1,step=FFT_N/len;
  for(int base=0;base<FFT_N;base+=len)for(int j=0;j<half;++j){const float wr=tw_re[j*step],wi=tw_im[j*step];const int a=base+j,b=a+half;
   const float tr=re[b]*wr-im[b]*wi,ti=re[b]*wi+im[b]*wr;re[b]=re[a]-tr;im[b]=im[a]-ti;re[a]+=tr;im[a]+=ti;}}
}
void music_viz_wave(float* out,int n){
 constexpr int SPAN=1024;
 pthread_mutex_lock(&tap_lock);
 const bool fresh=tap_count>=SPAN&&now_us()-tap_at_us<STALE_US;const int start=(tap_wr-SPAN)&(TAP_CAP-1);
 for(int i=0;i<n;++i)out[i]=fresh?tap[(start+(int)((int64_t)i*SPAN/n))&(TAP_CAP-1)]:0.0f;
 pthread_mutex_unlock(&tap_lock);
}
void music_viz_bands(float* out){
 static float re[FFT_N],im[FFT_N];
 build_tables();
 bool fresh=false;
 pthread_mutex_lock(&tap_lock);
 if(tap_count>=FFT_N&&now_us()-tap_at_us<STALE_US){
  const int start=(tap_wr-FFT_N)&(TAP_CAP-1);
  for(int i=0;i<FFT_N;++i){re[brev[i]]=tap[(start+i)&(TAP_CAP-1)]*hann[i];im[brev[i]]=0;}
  fresh=true;
 }
 pthread_mutex_unlock(&tap_lock);
 float raw[MUSIC_VIZ_BANDS]={};
 if(fresh){
  fft(re,im);float mx=0;
  for(int i=0;i<MUSIC_VIZ_BANDS;++i){
   if(band_hi[i]-band_lo[i]==1){
    // A narrow band: interpolate at its fractional centre bin.
    const int k0=(int)band_ctr[i];const float fr=band_ctr[i]-k0;
    const float m0=sqrtf(re[k0]*re[k0]+im[k0]*im[k0]),m1=sqrtf(re[k0+1]*re[k0+1]+im[k0+1]*im[k0+1]);raw[i]=m0+(m1-m0)*fr;
   }else{float acc=0;for(int k=band_lo[i];k<band_hi[i];++k)acc+=sqrtf(re[k]*re[k]+im[k]*im[k]);raw[i]=acc/(band_hi[i]-band_lo[i]);}
   if(raw[i]>mx)mx=raw[i];}
  // Normalise to the frame's loudest band, with a floor so digital silence
  // does not amplify noise into bars.
  if(mx<0.25f)memset(raw,0,sizeof raw);else for(float& v:raw){v/=mx;if(v>1)v=1;}
  memcpy(last_raw,raw,sizeof raw);
 }
 for(int i=0;i<MUSIC_VIZ_BANDS;++i){smooth[i]+=(raw[i]-smooth[i])*(raw[i]>smooth[i]?0.55f:0.13f);out[i]=smooth[i];}
}

#ifndef HOST_TEST
// ---- the tap -----------------------------------------------------------------
// MPV's ao_sdl opens its device with SDL_OpenAudio and a callback that fills
// each buffer. The linker sends that call here (--wrap=SDL_OpenAudio): our
// callback runs MPV's, then copies what it wrote. Nothing in the audio path
// changes; the copy is a few thousand samples per buffer.
//
// SDL's own headers do not build against this sysroot (no sys/endian.h), so
// the one struct and the format bits used here are spelled out from SDL2's
// SDL_audio.h, whose layout is part of SDL2's stable ABI.
typedef void (*SDL_AudioCallback)(void* userdata,uint8_t* stream,int len);
typedef uint16_t SDL_AudioFormat;
struct SDL_AudioSpec{int freq;SDL_AudioFormat format;uint8_t channels,silence;uint16_t samples,padding;uint32_t size;SDL_AudioCallback callback;void* userdata;};
#define SDL_AUDIO_BITSIZE(x) ((x)&0xFF)
#define SDL_AUDIO_ISFLOAT(x) ((x)&(1<<8))
typedef uint8_t Uint8;
extern "C" int __real_SDL_OpenAudio(SDL_AudioSpec* desired,SDL_AudioSpec* obtained);
static SDL_AudioCallback mpv_callback=nullptr;
static void* mpv_userdata=nullptr;
static SDL_AudioFormat tap_format=0x8010;  // AUDIO_S16LSB
static int tap_channels=2;
static std::atomic<uint64_t> last_sound_ms{0};
uint64_t audio_last_sound_ms(){return last_sound_ms;}
static void tap_callback(void* userdata,Uint8* stream,int len){
 (void)userdata;if(mpv_callback)mpv_callback(mpv_userdata,stream,len);else memset(stream,0,len);
 // MPV fills an underrun with zeros: any other byte is sound.
 for(int i=0;i<len;i+=16)if(stream[i]){last_sound_ms=mono_ms();break;}
 const int bytes=SDL_AUDIO_BITSIZE(tap_format)/8,frame=bytes*tap_channels;if(frame<=0)return;
 float lr[512*2];int frames=len/frame,done=0;
 while(done<frames){
  const int n=frames-done<512?frames-done:512;
  for(int i=0;i<n;++i)for(int c=0;c<2;++c){
   const Uint8* p=stream+(size_t)(done+i)*frame+(size_t)(c<tap_channels?c:0)*bytes;float v;
   if(SDL_AUDIO_ISFLOAT(tap_format)){float f;memcpy(&f,p,4);v=f;}
   else if(bytes==4){int32_t s;memcpy(&s,p,4);v=s/2147483648.0f;}
   else if(bytes==2){int16_t s;memcpy(&s,p,2);v=s/32768.0f;}
   else v=((int)*p-128)/128.0f;
   lr[i*2+c]=v;}
  music_viz_push(lr,n);done+=n;}
}
extern "C" int __wrap_SDL_OpenAudio(SDL_AudioSpec* desired,SDL_AudioSpec* obtained){
 if(!desired)return __real_SDL_OpenAudio(desired,obtained);
 mpv_callback=desired->callback;mpv_userdata=desired->userdata;
 if(mpv_callback){desired->callback=tap_callback;desired->userdata=nullptr;}
 const int rc=__real_SDL_OpenAudio(desired,obtained);
 // With `obtained`, SDL plays the format it reports there; without, it converts to `desired`.
 const SDL_AudioSpec& used=obtained&&rc>=0?*obtained:*desired;tap_format=used.format;tap_channels=used.channels?used.channels:2;
 desired->callback=mpv_callback;desired->userdata=mpv_userdata;
 return rc;
}
#endif
