#include "avplayer_memory.h"
#include <algorithm>
#include <stdio.h>
void* AvMemoryPool::allocate(uint32_t alignment,uint32_t size,bool texture){
 size_t aligned=std::max<size_t>(alignment,0x4000),bytes=((size_t)size+0x3fff)&~size_t(0x3fff);
 if(!size||(aligned&(aligned-1))){ops.log("AvPlayer memory: invalid size/alignment");return nullptr;}
 int64_t physical=0;void* address=nullptr;
 int result=ops.allocate(bytes,aligned,texture?3:0,&physical);
 if(result>=0){result=ops.map(&address,bytes,0x33,physical,aligned);if(result<0||!address){ops.release(physical,bytes);address=nullptr;}}
 if(size>=1024*1024||!address){char message[180];snprintf(message,sizeof message,"AvPlayer memory: %s bytes=%u align=%u mapped=%s result=0x%08x",texture?"texture":"shared CPU",size,alignment,address?"yes":"no",(unsigned)result);ops.log(message);}
 if(!address)return nullptr;
 pthread_mutex_lock(&lock);blocks[address]={physical,bytes};pthread_mutex_unlock(&lock);
 return address;
}
void AvMemoryPool::release(void* pointer){
 if(!pointer)return;
 pthread_mutex_lock(&lock);auto found=blocks.find(pointer);
 if(found==blocks.end()){pthread_mutex_unlock(&lock);ops.log("AvPlayer memory: unknown allocation");return;}
 auto allocation=found->second;
 int result=ops.unmap(pointer,allocation.size);
 if(result>=0){blocks.erase(found);result=ops.release(allocation.offset,allocation.size);}
 pthread_mutex_unlock(&lock);
 if(result<0){char message[100];snprintf(message,sizeof message,"AvPlayer memory: release failed 0x%08x",(unsigned)result);ops.log(message);}
}
