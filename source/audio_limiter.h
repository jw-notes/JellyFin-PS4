#pragma once
// A safety limiter for 5.1 and 7.1 sources mixed down to stereo.
//
// MPV's mixer adds the centre and surrounds to the fronts without scaling them
// down, which keeps a film's dialogue as loud as a stereo track's but lets a
// loud passage pass full scale (+5 dB measured on a loud 5.1 fixture). This
// turns only those passages down: below the ceiling every sample passes
// through unchanged, bit for bit. Attack is immediate, so nothing passes the
// ceiling; release is about 150 ms, so it does not pump.
#include <math.h>
#include <stdint.h>
struct StereoLimiter{
 static constexpr float CEILING=0.98f;
 static constexpr float RELEASE_STEP=1.0f/(0.15f*48000.0f);
 float gain=1.0f,peak_in=0.0f;
 uint64_t limited_frames=0;
 void reset(){gain=1.0f;peak_in=0.0f;limited_frames=0;}
 void process(float* lr,int frames){
  for(int i=0;i<frames;++i){
   const float left=fabsf(lr[i*2]),right=fabsf(lr[i*2+1]),peak=left>right?left:right;
   if(peak>peak_in)peak_in=peak;
   const float want=peak>CEILING?CEILING/peak:1.0f;
   if(want<gain)gain=want;
   // Close to 1 the step falls below a float's precision and the gain would stop
   // short for good: 0.999 (-0.01 dB) is where it finishes the climb in one step.
   else if(gain<1.0f){gain+=(want-gain)*RELEASE_STEP;if(gain>0.999f&&want>=1.0f)gain=1.0f;}
   if(gain<1.0f){lr[i*2]*=gain;lr[i*2+1]*=gain;++limited_frames;}
  }
 }
};
