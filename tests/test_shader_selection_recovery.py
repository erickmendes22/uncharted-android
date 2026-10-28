#!/usr/bin/env python3
"""Execute production selection recovery before/after nested guest-table reads."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_cache.cpp').read_text()
a=s.index('bool PipelineCache::PrepareGraphicsPipeline(');brace=s.index('{',a);b=brace+1;depth=1
while depth:
 depth+=(s[b]=='{')-(s[b]=='}');b+=1
f=s[a:b]
code=r'''
#include <cassert>
#include <csetjmp>
#include <cstdio>
thread_local sigjmp_buf* runtime_fault_recover=nullptr;
struct Regs {struct {bool hs_en=false;} stage_enable;};
struct Worker {bool failed=false,compile=false;};
struct PipelineSelection {Regs* regs;Worker* worker;};
struct PipelineCache {
 bool warm=true,worker_compile=true;int mode=0,built=0;
 bool IsWarm(){return warm;}
 bool RefreshGraphicsKey(PipelineSelection&){
  assert(runtime_fault_recover);
  if(mode==1)siglongjmp(*runtime_fault_recover,1); // GetParams, before inner recovery
  if(mode==2){ // a nested successful lookup must leave outer selection recoverable
   sigjmp_buf inner;auto* previous=runtime_fault_recover;
   if(sigsetjmp(inner,0))assert(false);
   runtime_fault_recover=&inner;runtime_fault_recover=previous;
   siglongjmp(*runtime_fault_recover,1);
  }
  return mode!=3;
 }
 void EnsureWorkerPipeline(PipelineSelection&){assert(!runtime_fault_recover);++built;}
 bool PrepareGraphicsPipeline(PipelineSelection&);
};
FUNCTION
int main(){
 PipelineCache cache;Regs regs;Worker worker;PipelineSelection sel{&regs,&worker};
 assert(cache.PrepareGraphicsPipeline(sel)&&cache.built==1&&!runtime_fault_recover);
 for(int mode:{1,2}){
  cache.mode=mode;worker.failed=false;
  assert(!cache.PrepareGraphicsPipeline(sel)&&worker.failed);
  assert(!runtime_fault_recover&&cache.built==1);
 }
 cache.mode=3;worker.failed=false;assert(!cache.PrepareGraphicsPipeline(sel));
 assert(!runtime_fault_recover&&cache.built==1);
 cache.warm=false;assert(!cache.PrepareGraphicsPipeline(sel));
 puts("PASS: stale shader reads before/after nested selection recover without publishing a pipeline or leaving recovery pointers active");
}
'''.replace('FUNCTION',f).replace('#include <cstdio>','#include <cstdio>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=5)
