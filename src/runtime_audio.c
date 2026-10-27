/* libSceAudioOut on SDL3 audio streams. sceAudioOutOutput returns once per
 * buffer period on a steady clock, like PS4 hardware: FMOD's output thread
 * reads 256-frame slots out of its mixer's 512-frame ring, and returning in
 * bursts (whenever the device pulls a quantum) lets it overtake the mixer and
 * play stale halves - a click every 10.7 ms. The lowest SDL queue level of
 * each window is steered to two buffers (silence prefill on underrun, ±3%
 * cadence corrections), which also absorbs device clock drift. Without an audio device (headless runs, BB_AUDIO=none)
 * ports only follow the clock. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <SDL3/SDL.h>

#define PORTS 25
#define ERR_NOT_OPENED ((int32_t)0x80260001)
#define ERR_INVALID_PORT ((int32_t)0x80260003)
#define ERR_INVALID_POINTER ((int32_t)0x80260004)
#define ERR_PORT_FULL ((int32_t)0x80260005)
#define ERR_INVALID_SIZE ((int32_t)0x80260006)
#define ERR_INVALID_FORMAT ((int32_t)0x80260007)
#define ERR_INVALID_FREQ ((int32_t)0x80260008)
#define ERR_INVALID_VOLUME ((int32_t)0x80260009)
#define ERR_INVALID_TYPE ((int32_t)0x8026000A)
#define ERR_ALREADY_INIT ((int32_t)0x8026000E)
#define ERR_NOT_INIT ((int32_t)0x8026000F)
#define VOLUME_0DB 32768

typedef struct {
    int used, type, channels, is_float, frames, sample_bytes, std_layout;
    int fold_stereo; /* 8 channels folded to stereo here for a device with fewer channels */
    int32_t volume[8];
    SDL_AudioStream *stream;
    uint64_t next_deadline_ns; /* next return of sceAudioOutOutput */
    int64_t adjust_ns; int window_min, window_count; /* queue level control */
    uint64_t last_output_us;
    FILE *dump;                     /* BB_AUDIO_DUMP: raw converted PCM per port */
    int stats;
    uint64_t stat_start_ns, stat_last_ns, stat_max_gap_ns; /* BB_AUDIO_STATS */
    unsigned stat_starved, stat_buffers; int stat_min_queued;
    int generation; unsigned drops; /* g2-arm64: the foreground seen last; stale queues dropped */
    pthread_mutex_t lock; /* lifetime is the process, never memset/reinitialized on reopen */
} Port;
typedef struct { uint16_t output; uint8_t channel, reserved; int16_t volume; uint16_t reroute; uint64_t flag, reserved64[2]; } PortState;
_Static_assert(sizeof(PortState)==32,"AudioOut port state layout");

static pthread_mutex_t table_lock=PTHREAD_MUTEX_INITIALIZER;
static Port ports[PORTS];
static atomic_int initialized;
static int sdl_ready=-1;
static atomic_size_t buffers_out, ports_opened;
static pthread_once_t port_locks_once=PTHREAD_ONCE_INIT;
static void init_port_locks(void) {
    for (int i=0;i<PORTS;++i) pthread_mutex_init(&ports[i].lock,NULL);
}
/* g2-arm64: Android's app lifecycle (gpu/shim/window.cpp). In the background SDL pauses the
   device while sceAudioOutOutput keeps the game's clock: the queue grew by every second the CPU
   stayed awake (26 s after the sleep button) and played that late afterwards, the cadence
   corrections drain 1/33 of a backlog per second. Nothing is queued in the background; the first
   output after it drops what is still queued. */
static atomic_int app_background, foreground_generation;
void runtime_audio_background(int background) {
    atomic_store(&app_background,background);
    if (!background) atomic_fetch_add(&foreground_generation,1);
}

static uint64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000000000u+(uint64_t)t.tv_nsec; }
static void sleep_until(uint64_t deadline) {
#ifdef _WIN32
    /* winpthreads' clock_nanosleep only takes CLOCK_REALTIME (EINVAL otherwise). */
    for (uint64_t now=now_ns(); now<deadline; now=now_ns()) compat_sleep_ns(deadline-now);
#else
    struct timespec t={(time_t)(deadline/1000000000u),(long)(deadline%1000000000u)};
    while (clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&t,NULL)) {}
#endif
}
static int sdl_audio(void) {
    if (sdl_ready<0) {
        const char *mode=getenv("BB_AUDIO");
        sdl_ready = (!mode || strcmp(mode,"none")) && SDL_InitSubSystem(SDL_INIT_AUDIO);
        printf("Runtime: audio backend %s\n", sdl_ready ? SDL_GetCurrentAudioDriver() : "timer (silent)");
    }
    return sdl_ready;
}
static int port_range(int type, int *first, int *last) {
    switch (type) {
    case 0: *first=0; *last=7; return 1;      /* main */
    case 1: *first=8; *last=8; return 1;      /* bgm */
    case 2: *first=9; *last=12; return 1;     /* voice */
    case 3: *first=13; *last=16; return 1;    /* personal */
    case 4: *first=17; *last=20; return 1;    /* pad speaker */
    case 126: *first=21; *last=22; return 1;  /* audio3d */
    case 127: *first=23; *last=24; return 1;  /* aux */
    default: return 0;
    }
}
static Port *port_of(int32_t handle, int32_t *error) {
    int id=handle & 0xff;
    if ((handle & 0x3f000000)!=0x20000000 || id>=PORTS) { *error=ERR_INVALID_PORT; return NULL; }
    if (!ports[id].used) { *error=ERR_NOT_OPENED; return NULL; }
    return &ports[id];
}

/* Pin the port before releasing the table. Close/reopen must not invalidate a
   stream or overwrite a mutex between handle lookup and acquisition. */
static Port *lock_port(int32_t handle, int32_t *error) {
    pthread_mutex_lock(&table_lock);
    Port *p=port_of(handle,error);
    if (p) pthread_mutex_lock(&p->lock);
    pthread_mutex_unlock(&table_lock);
    return p;
}

static ABI int32_t audio_init(void) {
    pthread_once(&port_locks_once,init_port_locks);
    pthread_mutex_lock(&table_lock);
    int32_t r=initialized ? ERR_ALREADY_INIT : 0;
    initialized=1;
    pthread_mutex_unlock(&table_lock);
    return r;
}
static ABI int32_t audio_open(int32_t user, int32_t type, int32_t index, uint32_t length, uint32_t freq, uint32_t param) {
    (void)user; (void)index;
    if (!initialized) return ERR_NOT_INIT;
    if (!length || length>2048 || (length & 0xff)) return ERR_INVALID_SIZE;
    if (freq!=48000) return ERR_INVALID_FREQ;
    uint32_t format=param & 0xff;
    if (format>7) return ERR_INVALID_FORMAT;
    int first, last;
    if (!port_range(type,&first,&last)) return ERR_INVALID_TYPE;
    static const int channels[8]={1,2,8,1,2,8,8,8};
    pthread_mutex_lock(&table_lock);
    int id=-1;
    for (int i=first;i<=last;++i) if (!ports[i].used) { id=i; break; }
    if (id<0) { pthread_mutex_unlock(&table_lock); return ERR_PORT_FULL; }
    Port *p=&ports[id];
    pthread_mutex_lock(&p->lock);
    memset(p,0,offsetof(Port,lock));
    p->used=1; p->type=type; p->channels=channels[format]; p->is_float=format>=3 && format!=6;
    p->generation=atomic_load(&foreground_generation);
    p->sample_bytes=p->is_float ? 4 : 2; p->frames=(int)length; p->std_layout=format>=6;
    for (int c=0;c<8;++c) p->volume[c]=VOLUME_0DB;
    if (sdl_audio()) {
        /* The games are told the output is stereo (sceAudioOutGetPortState) and fill only the
           front pair of 8-channel buffers. SDL's 7.1 to stereo downmix scales that pair by
           about 0.3: on a stereo device everything played far too quietly. Fold here instead. */
        SDL_AudioSpec device={0};
        p->fold_stereo = p->channels==8 &&
            SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&device,NULL) && device.channels<8;
        SDL_AudioSpec spec={p->is_float ? SDL_AUDIO_F32 : SDL_AUDIO_S16, p->fold_stereo ? 2 : p->channels, 48000};
        int raised=runtime_thread_system_priority_begin(); /* g2-arm64: the device's threads start here */
        p->stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);
        if (p->stream) SDL_ResumeAudioStreamDevice(p->stream);
        else fprintf(stderr,"Runtime: SDL audio stream failed (%s); port %d uses the timer sink\n",SDL_GetError(),id);
        runtime_thread_system_priority_end(raised);
    }
    p->stats=getenv("BB_AUDIO_STATS")!=NULL;
    const char *dump=getenv("BB_AUDIO_DUMP");
    if (dump) {
        char path[4096];
        snprintf(path,sizeof(path),"%s.port%d.%dch.%s",dump,id,p->channels,p->is_float ? "f32" : "s16");
        p->dump=fopen(path,"wb");
    }
    ++ports_opened;
    printf("Runtime: audio port %d opened (type %d, %d ch, %s, %u frames%s)\n",id,type,p->channels,p->is_float ? "float" : "s16",length,
           p->fold_stereo ? ", folded to stereo" : "");
    pthread_mutex_unlock(&p->lock);
    pthread_mutex_unlock(&table_lock);
    return (type<<16) | id | 0x20000000;
}
static ABI int32_t audio_close(int32_t handle) {
    pthread_mutex_lock(&table_lock);
    int32_t error=0;
    Port *p=port_of(handle,&error);
    if (p) {
        pthread_mutex_lock(&p->lock);
        if (p->stream) SDL_DestroyAudioStream(p->stream);
        if (p->dump) fclose(p->dump);
        p->dump=NULL;
        p->stream=NULL; p->used=0;
        pthread_mutex_unlock(&p->lock);
    }
    pthread_mutex_unlock(&table_lock);
    return p ? 0 : error;
}
/* `pace`: wait for this port's next period. sceAudioOutOutputs waits once for all its ports. */
static int32_t output_port(int32_t handle, const void *data, int pace) {
    int32_t error=0;
    Port *p=lock_port(handle,&error);
    if (!p) return error;
    size_t samples=(size_t)p->frames*(size_t)p->channels, bytes=samples*(size_t)p->sample_bytes;
    uint64_t period=(uint64_t)p->frames*1000000000u/48000u;
    if (data) {
        /* Apply per-channel volume and map PS4 8ch (L R C LFE SL SR BL BR) to SDL 7.1 order. */
        unsigned char converted[2048*8*4];
        static const int remap[8]={0,1,2,3,6,7,4,5};
        for (size_t f=0;f<(size_t)p->frames;++f) for (int c=0;c<p->channels;++c) {
            int target=p->channels==8 && !p->std_layout ? remap[c] : c;
            size_t from=f*(size_t)p->channels+(size_t)c, to=f*(size_t)p->channels+(size_t)target;
            float gain=(float)p->volume[c]/VOLUME_0DB;
            if (p->is_float) { float v; memcpy(&v,(const char *)data+from*4,4); v*=gain; memcpy(converted+to*4,&v,4); }
            else { int16_t v; memcpy(&v,(const char *)data+from*2,2); v=(int16_t)((float)v*gain); memcpy(converted+to*2,&v,2); }
        }
        if (p->dump) fwrite(converted,1,bytes,p->dump);
        /* Stereo device: FL/FR at full level, centre and surrounds at -3 dB (LFE dropped). */
        const unsigned char *queue_data=converted;
        size_t queue_bytes=bytes;
        unsigned char folded[2048*2*4];
        if (p->fold_stereo) {
            for (size_t f=0;f<(size_t)p->frames;++f) {
                float s[8];
                for (int c=0;c<8;++c) {
                    if (p->is_float) memcpy(&s[c],converted+(f*8+(size_t)c)*4,4);
                    else { int16_t v; memcpy(&v,converted+(f*8+(size_t)c)*2,2); s[c]=(float)v/32768.0f; }
                }
                /* SDL 7.1 order: FL FR FC LFE BL BR SL SR */
                float l=s[0]+0.7071f*(s[2]+s[4]+s[6]), r=s[1]+0.7071f*(s[2]+s[5]+s[7]);
                l=l>1.0f ? 1.0f : l<-1.0f ? -1.0f : l;
                r=r>1.0f ? 1.0f : r<-1.0f ? -1.0f : r;
                if (p->is_float) { memcpy(folded+f*8,&l,4); memcpy(folded+f*8+4,&r,4); }
                else { int16_t a=(int16_t)(l*32767.0f), b=(int16_t)(r*32767.0f); memcpy(folded+f*4,&a,2); memcpy(folded+f*4+2,&b,2); }
            }
            queue_data=folded;
            queue_bytes=(size_t)p->frames*2*(size_t)p->sample_bytes;
        }
        if (p->stats) {
            uint64_t now=now_ns();
            if (!p->stat_start_ns) { p->stat_start_ns=now; p->stat_min_queued=1<<30; }
            else if (now-p->stat_last_ns>p->stat_max_gap_ns) p->stat_max_gap_ns=now-p->stat_last_ns;
            p->stat_last_ns=now; ++p->stat_buffers;
            if (p->stream) {
                int queued=SDL_GetAudioStreamQueued(p->stream);
                if (queued<p->stat_min_queued) p->stat_min_queued=queued;
                if (queued==0) ++p->stat_starved;
            }
            if (now-p->stat_start_ns>=5000000000u) {
                printf("Audio stats port %d: %u buffers/5s (expected %u), max gap %.2f ms, min queued %d bytes, starved %u\n",
                       (int)(p-ports),p->stat_buffers,(unsigned)(5u*48000u/(unsigned)p->frames),p->stat_max_gap_ns/1e6,
                       p->stat_min_queued,p->stat_starved);
                p->stat_start_ns=now; p->stat_max_gap_ns=0; p->stat_starved=0; p->stat_buffers=0; p->stat_min_queued=1<<30;
            }
        }
        if (pace) {
            uint64_t now=now_ns();
            if (!p->next_deadline_ns || now>p->next_deadline_ns+8*period) p->next_deadline_ns=now; /* start or stall: restart cadence */
            else sleep_until(p->next_deadline_ns);
            p->next_deadline_ns+=period;
        }
        if (p->stream && !atomic_load(&app_background)) {
            /* The device drains the queue in quanta (21 ms on PipeWire), so the level
               is a sawtooth: its minimum over ~32 buffers is what gets controlled. */
            static const unsigned char silence[2048*8*4];
            int qb=(int)queue_bytes, low=2*qb, queued=SDL_GetAudioStreamQueued(p->stream);
            /* g2-arm64: back from the background, or 250 ms over the target (the device stopped
               taking audio: a device switch): what is queued is stale, drop it. */
            int generation=atomic_load(&foreground_generation), frame=qb/p->frames;
            int resumed=p->generation!=generation, stalled=queued>low+12000*frame;
            if (resumed || stalled) {
                if (resumed || !(p->drops++ & 63))
                    printf("Audio: port %d: %s, %d ms of queued audio dropped\n",(int)(p-ports),
                           resumed ? "back from the background" : "the device stopped taking audio",queued/frame/48);
                p->generation=generation;
                SDL_ClearAudioStream(p->stream);
                queued=0; p->window_count=0; p->adjust_ns=0;
            }
            if (queued<qb) {
                SDL_PutAudioStreamData(p->stream,silence,low-queued<qb ? low-queued : qb);
                queued=low;
            }
            if (!p->window_count || queued<p->window_min) p->window_min=queued;
            if (++p->window_count==32) {
                p->adjust_ns=p->window_min>low+2*qb ? (int64_t)(period/32) : p->window_min<low ? -(int64_t)(period/32) : 0;
                p->window_count=0;
            }
            p->next_deadline_ns+=(uint64_t)p->adjust_ns;
            SDL_PutAudioStreamData(p->stream,queue_data,qb);
        }
        ++buffers_out;
    }
    p->last_output_us=now_ns()/1000;
    pthread_mutex_unlock(&p->lock);
    return data ? (int32_t)samples : 0;
}
static ABI int32_t audio_output(int32_t handle, const void *data) { return output_port(handle,data,1); }
typedef struct { int32_t handle; const void *data; } OutputParam;
static ABI int32_t audio_outputs(const OutputParam *params, uint32_t count) {
    if (!params) return ERR_INVALID_POINTER;
    if (!count || count>PORTS) return ERR_PORT_FULL;
    int32_t result=0;
    /* One period per call for all ports (FMOD feeds main and BGM together): pacing every
       port would halve the rate and starve both. */
    for (uint32_t i=0;i<count;++i) { int32_t r=output_port(params[i].handle,params[i].data,i==0); if (r<0) return r; result=r; }
    return result;
}
static ABI int32_t audio_volume(int32_t handle, int32_t flags, const int32_t *volume) {
    int32_t error=0;
    if (!volume) return ERR_INVALID_POINTER;
    Port *p=lock_port(handle,&error);
    if (!p) return error;
    for (int c=0;c<8;++c) if (flags & (1<<c)) {
        if (volume[c]<0 || volume[c]>VOLUME_0DB) { pthread_mutex_unlock(&p->lock); return ERR_INVALID_VOLUME; }
        p->volume[c]=volume[c];
    }
    pthread_mutex_unlock(&p->lock);
    return 0;
}
static ABI int32_t audio_state(int32_t handle, PortState *state) {
    int32_t error=0;
    if (!state) return ERR_INVALID_POINTER;
    Port *p=lock_port(handle,&error);
    if (!p) return error;
    memset(state,0,sizeof(*state));
    switch (p->type) {
    case 2: case 3: state->output=0x40; state->channel=1; break;          /* headphone */
    case 4: state->output=0x04; state->channel=1; state->volume=127; break; /* pad speaker */
    default: state->output=0x01; state->channel=(uint8_t)(p->channels>2 ? 2 : p->channels); break;
    }
    pthread_mutex_unlock(&p->lock);
    return 0;
}
static ABI int32_t audio_last_time(int32_t handle, uint64_t *time) {
    int32_t error=0;
    if (!time) return ERR_INVALID_POINTER;
    Port *p=lock_port(handle,&error);
    if (!p) return error;
    *time=p->last_output_us;
    pthread_mutex_unlock(&p->lock);
    return 0;
}

static const RuntimeExport exports[]={
    {"sceAudioOutInit",audio_init}, {"sceAudioOutOpen",audio_open}, {"sceAudioOutClose",audio_close},
    {"sceAudioOutOutput",audio_output}, {"sceAudioOutOutputs",audio_outputs},
    {"sceAudioOutSetVolume",audio_volume}, {"sceAudioOutGetPortState",audio_state},
    {"sceAudioOutGetLastOutputTime",audio_last_time},
};
uintptr_t runtime_audio_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_audio_report(void) { printf("Runtime: audio ports opened=%zu, buffers output=%zu\n",ports_opened,buffers_out); }
