#define _GNU_SOURCE
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <SDL3/SDL.h>
static int checked_mutex_lock(pthread_mutex_t*);
static int checked_mutex_unlock(pthread_mutex_t*);
#define pthread_mutex_lock checked_mutex_lock
#define pthread_mutex_unlock checked_mutex_unlock
#include "../src/runtime_audio.c"
#undef pthread_mutex_lock
#undef pthread_mutex_unlock
static _Thread_local int rendering,port_locks,queried_handle;
static int checked_mutex_lock(pthread_mutex_t* mutex){
    int r=pthread_mutex_lock(mutex);
    if(!r && mutex!=&table_lock)++port_locks;
    return r;
}
static int checked_mutex_unlock(pthread_mutex_t* mutex){
    // A live handle lookup must never escape the table without a pinned port.
    // This deterministic regression assertion fails on the pre-fix implementation.
    int32_t error=0;
    if(rendering && mutex==&table_lock && port_of(queried_handle,&error))assert(port_locks>0);
    if(mutex!=&table_lock)--port_locks;
    return pthread_mutex_unlock(mutex);
}
struct SDL_AudioStream { atomic_int alive; struct SDL_AudioStream* next; };
static struct SDL_AudioStream* allocated;
static atomic_int current_handle,stop,stream_count,destroy_count;
static pthread_mutex_t allocated_lock=PTHREAD_MUTEX_INITIALIZER;
bool SDL_InitSubSystem(SDL_InitFlags flags){(void)flags;return true;}
const char* SDL_GetCurrentAudioDriver(void){return "mock";}
const char* SDL_GetError(void){return "mock error";}
bool SDL_GetAudioDeviceFormat(SDL_AudioDeviceID id,SDL_AudioSpec* spec,int* frames){(void)id;(void)frames;spec->channels=2;return true;}
SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID id,const SDL_AudioSpec* spec,SDL_AudioStreamCallback cb,void* ud){
    (void)id;(void)spec;(void)cb;(void)ud;struct SDL_AudioStream* s=calloc(1,sizeof(*s));assert(s);atomic_store(&s->alive,1);
    pthread_mutex_lock(&allocated_lock);s->next=allocated;allocated=s;pthread_mutex_unlock(&allocated_lock);++stream_count;return s;
}
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream* s){assert(atomic_load(&s->alive));return true;}
void SDL_DestroyAudioStream(SDL_AudioStream* s){assert(atomic_exchange(&s->alive,0)==1);++destroy_count;}
int SDL_GetAudioStreamQueued(SDL_AudioStream* s){assert(atomic_load(&s->alive));sched_yield();assert(atomic_load(&s->alive));return 0;}
bool SDL_ClearAudioStream(SDL_AudioStream* s){assert(atomic_load(&s->alive));return true;}
bool SDL_PutAudioStreamData(SDL_AudioStream* s,const void* data,int len){(void)data;(void)len;assert(atomic_load(&s->alive));sched_yield();assert(atomic_load(&s->alive));return true;}
int runtime_thread_system_priority_begin(void){return 0;}
void runtime_thread_system_priority_end(int old){(void)old;}
uintptr_t runtime_lookup(const RuntimeExport* t,size_t n,const char* name){(void)t;(void)n;(void)name;return 0;}
static void* render(void* unused){
    (void)unused;rendering=1;float data[2048*8]={0};const int32_t volumes[8]={32768,32768,32768,32768,32768,32768,32768,32768};
    while(!atomic_load(&stop)) {
        const int h=atomic_load(&current_handle);queried_handle=h;if(h<0){sched_yield();continue;}
        output_port(h,data,0);audio_volume(h,255,volumes);PortState state;audio_state(h,&state);uint64_t at;audio_last_time(h,&at);
    }
    return NULL;
}
int main(void){
    assert(audio_init()==0);assert(audio_init()==ERR_ALREADY_INIT);
    atomic_store(&current_handle,-1);pthread_t workers[3];for(int n=0;n<3;++n)assert(!pthread_create(&workers[n],NULL,render,NULL));
    int retired=-1;
    for(int n=0;n<500;++n) {
        const int h=audio_open(1,0,0,n%2?1024:256,48000,5);assert(h>=0);
        if(retired>=0){PortState state;assert(audio_state(retired,&state)==ERR_NOT_OPENED);}
        atomic_store(&current_handle,h);
        for(int i=0;i<20;++i)sched_yield();
        if(n%25==0){runtime_audio_background(1);runtime_audio_background(0);}
        assert(audio_close(h)==0);atomic_store(&current_handle,-1);retired=h;
    }
    atomic_store(&stop,1);for(int n=0;n<3;++n)pthread_join(workers[n],NULL);
    assert(stream_count==500 && destroy_count==500);
    while(allocated){struct SDL_AudioStream* next=allocated->next;free(allocated);allocated=next;}
    puts("Audio lifetime: PASS (500 reopen cycles, 256/1024-frame buffers, stale handles rejected, concurrent queries, foreground transitions)");
}
