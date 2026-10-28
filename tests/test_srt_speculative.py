#!/usr/bin/env python3
"""Execute the production ARM SRT recovery method against a simulated stale guest pointer."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'gpu/shadps4/shader_recompiler/ir/passes/srt_interp.cpp').read_text()
a=s.index('bool RunSrtWalkerInterpreted(');brace=s.index('{',a);b=brace+1;depth=1
while depth:
 depth+=(s[b]=='{')-(s[b]=='}');b+=1
function=s[a:b]
code=r'''
#include <atomic>
#include <cassert>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
using u32=uint32_t;using PFN_SrtWalker=void(*)();
thread_local sigjmp_buf* runtime_fault_recover=nullptr;
enum class Kind {Copy,Mov};
struct Op {Kind kind=Kind::Mov;std::atomic<bool> zeroed=false;};
struct Program {bool supported=true;unsigned copies=0,flat_bytes=0;std::vector<Op> ops=std::vector<Op>(1);} program;
struct Settings {bool fex=false,stats=false,copies=false,check=false;} settings;
const Settings& GetSettings(){return settings;}
Program* GetProgram(PFN_SrtWalker){return &program;}
void CountWalk(const Program&){}
void CheckWalk(Program&,PFN_SrtWalker,const u32*,u32*,const std::vector<u32>&){}
bool fault=true;
void Execute(Program& p,const u32*,u32* out,bool,volatile size_t& op){
 op=0;
 if(fault&&!p.ops[0].zeroed.load())siglongjmp(*runtime_fault_recover,1);
 *out=p.ops[0].zeroed.load()?0:79;
}
FUNCTION
int main(){
 u32 out=9;sigjmp_buf worker;
 if(!sigsetjmp(worker,0)){
  runtime_fault_recover=&worker;
  RunSrtWalkerInterpreted(nullptr,nullptr,&out);
  assert(false);
 }
 assert(runtime_fault_recover==&worker);
 assert(!program.ops[0].zeroed.load()); // speculation never poisons the shared walker
 runtime_fault_recover=nullptr;fault=false;
 assert(RunSrtWalkerInterpreted(nullptr,nullptr,&out)&&out==79);
 fault=true;assert(RunSrtWalkerInterpreted(nullptr,nullptr,&out)&&out==0);
 assert(program.ops[0].zeroed.load()); // the real GPU thread keeps the existing fallback
 assert(runtime_fault_recover==nullptr);
 settings.fex=true;assert(!RunSrtWalkerInterpreted(nullptr,nullptr,&out));
 puts("PASS: speculative stale-pointer failure stays local; real shader walk and fallback preserved");
}
'''.replace('FUNCTION',function)
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=10)
