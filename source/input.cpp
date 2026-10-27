#include "input_model.h"
#include "mono_clock.h"
#include "platform.h"
#include "screensaver.h"
#include <time.h>
static UiInputFilter filter;
static uint64_t now_ms(){return mono_ms();}
void ui_input_sync(){filter.sync(buttons(),now_ms());}
uint32_t ui_input(){
 uint32_t raw=buttons();uint64_t now=now_ms();
 // Every screen samples input here, so the screensaver can start from any of
 // them. The press that wakes it is swallowed.
 if(screensaver_poll(raw,now)){filter.sync(buttons(),now_ms());return 0;}
 return filter.update(raw,now);
}
