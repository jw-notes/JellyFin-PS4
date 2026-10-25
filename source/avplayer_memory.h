#pragma once
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <pthread.h>
struct AvMemoryOps {
 int(*allocate)(size_t,size_t,int,int64_t*);
 int(*map)(void**,size_t,int,int64_t,size_t);
 int(*unmap)(void*,size_t);
 int(*release)(int64_t,size_t);
 void(*log)(const char*);
};
class AvMemoryPool {
 struct Allocation{int64_t offset;size_t size;};
 AvMemoryOps ops;
 std::map<void*,Allocation> blocks;
 pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
public:
 explicit AvMemoryPool(AvMemoryOps operations):ops(operations){}
 void* allocate(uint32_t alignment,uint32_t size,bool texture);
 void release(void* pointer);
};
