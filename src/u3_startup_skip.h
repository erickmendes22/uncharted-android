/* SPDX-License-Identifier: GPL-2.0-or-later
 * Stop only the two startup-logo movies once the movie reaches playback.
 * Uses the same Stop(1) virtual call as the game's own skip input; story movies
 * and the normal movie teardown/state transitions are preserved. */
#ifndef U3_STARTUP_SKIP_H
#define U3_STARTUP_SKIP_H
static size_t u3_emit_logo_skip(unsigned char *t,uint32_t metadata_offset,
                                uintptr_t player_slot,uintptr_t resume,uint32_t state_offset,
                                const unsigned char *tail,size_t tail_size) {
    size_t k=0,exits[32],ne=0;
#define U3_BYTES(...) do { const unsigned char b[]={__VA_ARGS__};memcpy(t+k,b,sizeof(b));k+=sizeof(b); } while(0)
#define U3_EXIT(op) do { U3_BYTES(0x0f,op);exits[ne++]=k;memset(t+k,0,4);k+=4; } while(0)
    /* State 5 is the verified playback case; initialization/teardown must finish. */
    U3_BYTES(0x41,0x83,0xbc,0x24);memcpy(t+k,&state_offset,4);k+=4;
    U3_BYTES(0x05);U3_EXIT(0x85);
    U3_BYTES(0x49,0x8b,0x84,0x24);memcpy(t+k,&metadata_offset,4);k+=4;
    U3_BYTES(0x48,0x85,0xc0);U3_EXIT(0x84);
    U3_BYTES(0x48,0x8b,0x40,0x08,0x48,0x85,0xc0);U3_EXIT(0x84);
    /* Compare only through a string's first mismatching byte, including NUL. No
     * qword probes beyond the end of a short name at a mapped-page boundary. */
    U3_BYTES(0x80,0x38,'b',0x0f,0x84);size_t bp_jump=k;memset(t+k,0,4);k+=4;
    U3_BYTES(0x80,0x38,'s',0x0f,0x84);size_t sce_jump=k;memset(t+k,0,4);k+=4;
    U3_BYTES(0xe9);exits[ne++]=k;memset(t+k,0,4);k+=4;
    int32_t delta=(int32_t)(k-bp_jump-4);memcpy(t+bp_jump,&delta,4);
    const char bp[]="bp-logos";
    for(unsigned i=1;i<sizeof(bp);++i){U3_BYTES(0x80,0x78,i,bp[i]);U3_EXIT(0x85);}
    U3_BYTES(0xe9);size_t stop_jump=k;memset(t+k,0,4);k+=4;
    delta=(int32_t)(k-sce_jump-4);memcpy(t+sce_jump,&delta,4);
    const char sce[]="sce-ndi-logos";
    for(unsigned i=1;i<sizeof(sce);++i){U3_BYTES(0x80,0x78,i,sce[i]);U3_EXIT(0x85);}
    delta=(int32_t)(k-stop_jump-4);memcpy(t+stop_jump,&delta,4);
    U3_BYTES(0x48,0xb8);memcpy(t+k,&player_slot,8);k+=8;
    U3_BYTES(0x48,0x8b,0x38,0x48,0x85,0xff);U3_EXIT(0x84);
    U3_BYTES(0x48,0x8b,0x07,0xbe,0x01,0x00,0x00,0x00,0xff,0x50,0x18);
    for(size_t i=0;i<ne;++i){delta=(int32_t)(k-exits[i]-4);memcpy(t+exits[i],&delta,4);}
    /* Replay the displaced stack-guard load/compare, including its flags. */
    if(tail_size){memcpy(t+k,tail,tail_size);k+=tail_size;}
    int32_t rel=(int32_t)((int64_t)resume-(int64_t)(uintptr_t)(t+k+5));
    U3_BYTES(0xe9);memcpy(t+k,&rel,4);k+=4;
#undef U3_EXIT
#undef U3_BYTES
    return k;
}
static void install_u3_skip_logos(uint64_t image_size) {
    const char *enabled=getenv("BB_U3_SKIP_LOGOS");
    if(!enabled || enabled[0]!='1')return;
    /* The successful Initialize branch: initial call -> failure check -> clear request ->
     * movie cue/audio setup -> state=1 -> common epilogue. Every displacement is derived. */
    const int64_t entry=find_unique_pattern(image_size,
        "4c89e7e8????????84c00f84????????41c68424702502000041c784247425020000000000"
        "6641c784247825020000004c89e7e8????????4c89e7e8????????41c784240c2202000000000041c784246c25020001000000e9????????");
    const int64_t meta=find_unique_pattern(image_size,
        "4c89e74489f6e8????????49898424????????4885c00f84????????8b73184c89e7e8????????49898424????????8b431c41898424????????488b7308");
    const int64_t stop=find_unique_pattern(image_size,
        "83e0fe83f8067515488d05????????488b38488b07be01000000ff5018");
    if(entry<0 || meta<0 || stop<0){puts("U3 startup logos: safe movie signatures unavailable; unchanged");return;}
    /* Derive the position and target of the original final five-byte jump. */
    size_t at_len=0,unused=0;
    runtime_find_guest_fix(image,(size_t)image_size,
        "4c89e7e8????????84c00f84????????41c68424702502000041c784247425020000000000"
        "6641c784247825020000004c89e7e8????????4c89e7e8????????41c784240c2202000000000041c784246c25020001000000e9????????",&unused,&at_len);
    unsigned char *init_jump=image+entry+at_len-5;
    int32_t rel;memcpy(&rel,init_jump+1,4);
    uintptr_t epilogue=(uintptr_t)(init_jump+5+rel);
    uint32_t state_offset;memcpy(&state_offset,init_jump-8,4);
    int32_t global_rel;memcpy(&global_rel,image+stop+11,4);
    uintptr_t player_slot=(uintptr_t)(image+stop+15+global_rel);
    uint32_t metadata_offset;memcpy(&metadata_offset,image+meta+15,4);
    int32_t init_rel;memcpy(&init_rel,image+entry+4,4);
    const int64_t init=entry+8+init_rel;
    /* Metadata read must belong to the same initializer, not another game's callback. */
    if(init<0 || meta<init || meta-init>1024 || metadata_offset>0x40000 ||
       state_offset!=metadata_offset+0x10c ||
       epilogue<(uintptr_t)image || epilogue>(uintptr_t)image+image_size-27 ||
       player_slot<(uintptr_t)image || player_slot>(uintptr_t)image+image_size-8){
        puts("U3 startup logos: movie ownership check failed; unchanged");return;
    }
    /* All playback paths return through this common guard epilogue. The former
     * one-shot post-initialize jump never retried once the decoder was ready. */
    static const unsigned char guard[]={0x48,0x8b,0x03,0x48,0x3b,0x45,0xd0,0x75,0x12,
        0x48,0x81,0xc4,0x38,0x01,0x00,0x00,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
    unsigned char *site=(unsigned char *)epilogue;
    if(memcmp(site,guard,sizeof(guard))){puts("U3 startup logos: update epilogue unavailable; unchanged");return;}
    unsigned char *thunk=near_image_memory(4096);
    if(!thunk){puts("U3 startup logos: no thunk memory; unchanged");return;}
    (void)u3_emit_logo_skip(thunk,metadata_offset,player_slot,epilogue+7,state_offset,guard,7);
    const int64_t delta=(int64_t)(uintptr_t)thunk-(int64_t)(uintptr_t)(site+5);
    if(delta<INT32_MIN || delta>INT32_MAX)return;
    rel=(int32_t)delta;site[0]=0xe9;memcpy(site+1,&rel,4);site[5]=site[6]=0x90;
    puts("U3 startup logos: logo-only Stop(1) at movie playback enabled");
}
#endif
