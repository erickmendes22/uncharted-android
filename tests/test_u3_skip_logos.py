"""Execute production x86 logo-only stop thunk; verify every affected control path."""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[1]
text=(root/'src/u3_startup_skip.h').read_text()
emitter=text[text.index('static size_t u3_emit_logo_skip'):text.index('static void install_u3_skip_logos')]
harness=r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
'''+emitter+r'''
static int stopped;
static void *expected;
static void stop(void *player,int flag){assert(player==expected && flag==1);++stopped;}
int main(void){
 unsigned char *code=mmap(NULL,4096,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(code!=MAP_FAILED);
 /* Caller's original stack alignment: preserve r12 then JMP, no extra return pushed. */
 const unsigned char begin[]={0x55,0x48,0x89,0xe5,0x53,0x41,0x54,0x48,0x83,0xec,0x30,
  0x49,0x89,0xfc,0x48,0x89,0xf3,0x48,0x8b,0x06,0x48,0x89,0x45,0xd0,0xe9};memcpy(code,begin,sizeof(begin));
 int32_t rel=128-(int32_t)(sizeof(begin)+4);memcpy(code+sizeof(begin),&rel,4);
 /* Resume at the original guard JNE: test replayed flags as well as registers. */
 const unsigned char end[]={0x75,0x0e,0xb8,0x01,0x00,0x00,0x00,0x48,0x83,0xc4,0x30,0x41,0x5c,0x5b,0x5d,0xc3,
  0x31,0xc0,0x48,0x83,0xc4,0x30,0x41,0x5c,0x5b,0x5d,0xc3};memcpy(code+64,end,sizeof(end));
 const unsigned char guard[]={0x48,0x8b,0x03,0x48,0x3b,0x45,0xd0};
 uint64_t cookie=0x123456789abcdef0;
 void *vt[4]={0,0,0,(void *)stop}, *player[]={vt}, *slot=player;expected=player;
 size_t n=u3_emit_logo_skip(code+128,0x10,(uintptr_t)&slot,(uintptr_t)(code+64),0x18,guard,sizeof(guard));assert(n<512);
 uint64_t obj[4]={0},meta[3]={0};obj[2]=(uintptr_t)meta;obj[3]=5;
 const char *names[]={"bp-logos","sce-ndi-logos","bp-logos-extra","sce-ndi-logos-extra","chapter01","","sce-ndi-logo","BP-LOGOS"};
 for(size_t i=0;i<sizeof(names)/sizeof(*names);++i){
  /* Readable padding like the game's metadata string table; no out-of-string page reads. */
  char name[64]={0};strcpy(name,names[i]);meta[1]=(uintptr_t)name;int before=stopped;
  assert(((int(*)(void *,void *))code)(obj,&cookie)==1);assert(stopped-before==(i<2));
 }
 meta[1]=0;assert(((int(*)(void *,void *))code)(obj,&cookie)==1);
 obj[2]=0;assert(((int(*)(void *,void *))code)(obj,&cookie)==1);
 assert(stopped==2);
 /* No metadata access or stop during idle/decoder startup/teardown. */
 for(unsigned state=0;state<8;++state){if(state==5)continue;
  obj[2]=1;obj[3]=state;assert(((int(*)(void *,void *))code)(obj,&cookie)==1);}
 assert(stopped==2);obj[3]=5;
 char *boundary=mmap(NULL,8192,3,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(boundary!=MAP_FAILED);
 assert(mprotect(boundary+4096,4096,PROT_NONE)==0);
 const char *shorts[]={"","b","s","sce-","bp-logo"};
 for(unsigned i=0;i<sizeof(shorts)/sizeof(*shorts);++i){
  size_t length=strlen(shorts[i])+1;char *name=boundary+4096-length;memcpy(name,shorts[i],length);
  obj[2]=(uintptr_t)meta;meta[1]=(uintptr_t)name;assert(((int(*)(void *,void *))code)(obj,&cookie)==1);
 }
 assert(stopped==2);
 puts("PASS: only playing startup logos stop through original Stop(1); guard flags replay; story/prefix/empty/null cases preserved");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(harness)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-O2',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# Original jump is preserved; safe rejection on ambiguous/missing signatures.
assert 'if(entry<0 || meta<0 || stop<0)' in text
assert 'delta<INT32_MIN || delta>INT32_MAX' in text
activity=(root/'android/scripts/g2/game_app/GameActivity.java').read_text()
assert '"BB_U3_SKIP_LOGOS","u3".equals(game) && options.getBoolean("patch_Skip startup logos",false)?"1":"0",true' in activity
