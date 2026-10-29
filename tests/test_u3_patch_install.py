"""Run production signature scanner/installers against relocated verified code fixtures."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
scanner=(root/'src/runtime_guest_fixes.c').read_text();scanner=scanner[scanner.index('size_t runtime_find_guest_fix('):]
c=r'''
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
static unsigned char *image;
'''+scanner+r'''
static int64_t find_unique_pattern(uint64_t size,const char *pattern){
 size_t at=0,len=0,n=runtime_find_guest_fix(image,size,pattern,&at,&len);return n==1?(int64_t)at:n>1?-2:-1;
}
static unsigned char *near_image_memory(size_t n){(void)n;return image+0x2000;}
#include "u3_startup_skip.h"
static void hex(unsigned char *out,const char *s){while(*s){char p[3]={s[0],s[1],0};*out++=(unsigned char)strtoul(p,NULL,16);s+=2;}}
int main(void){
 image=mmap(NULL,0x3000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(image!=MAP_FAILED);
 hex(image+0x800,"4c89e7e871f4ffff84c00f842204000041c68424702502000041c7842474250200000000006641c784247825020000004c89e7e8410500004c89e7e86906000041c784240c2202000000000041c784246c25020001000000e944040000");
 hex(image+0x512,"4c89e74489f6e843b1ffff49898424602402004885c00f84fd0000008b73184c89e7e827b1ffff49898424502402008b431c4189842458240200488b7308");
 hex(image+0x100,"83e0fe83f8067515488d053a6a2101488b38488b07be01000000ff5018");
 int32_t rel=0x400-(0x800+8);memcpy(image+0x804,&rel,4);
 rel=0x1800-(0x100+15);memcpy(image+0x100+11,&rel,4);
 rel=0x1000-(0x800+93);memcpy(image+0x800+89,&rel,4);
 hex(image+0x1000,"488b03483b45d075124881c4380100005b415c415d415e415f5dc3");
 setenv("BB_U3_SKIP_LOGOS","1",1);
 install_u3_skip_logos(0x2000);
 memcpy(&rel,image+0x800+89,4);assert(image+0x800+93+rel==image+0x1000);
 assert(image[0x1000]==0xe9 && image[0x1005]==0x90 && image[0x1006]==0x90);
 memcpy(&rel,image+0x1001,4);assert(image+0x1005+rel==image+0x2000);
 /* Unsupported/corrupt epilogue must remain untouched. */
 image[0x1000]=0xcc;install_u3_skip_logos(0x2000);assert(image[0x1000]==0xcc);
 puts("PASS: production U3 logo installer derives relocated targets from verified instruction fixtures");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(c)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-O2','-I'+str(root/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
