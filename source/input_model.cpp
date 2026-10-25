#include "input_model.h"
void UiInputFilter::sync(uint32_t raw,uint64_t now){held=raw;nav=raw&0xf0;repeat_at=now+350;released.fill(0);}
uint32_t UiInputFilter::update(uint32_t raw,uint64_t now){
 uint32_t events=0;
 // The pad already provides digital states. Waiting 65 ms before releasing
 // a latch swallowed legitimate quick taps and made the UI feel unresponsive.
 events=raw&~held;held=raw;
 uint32_t direction=raw&0xf0;
 if(direction!=nav){nav=direction;repeat_at=now+350;}
 else if(direction&&now>=repeat_at){events|=direction;repeat_at=now+110;}
 return events;
}
