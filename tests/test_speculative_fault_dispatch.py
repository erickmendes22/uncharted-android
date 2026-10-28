#!/usr/bin/env python3
"""Exercise the actual fault dispatcher with an owner waiting for a shader worker.
The old production function must reproduce the cycle; the patched one must recover.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'gpu/shim/bbgpu.cpp').read_text()
a=s.index('extern "C" int bbgpu_handle_fault(');brace=s.index('{',a);b=brace+1;depth=1
while depth:
 depth+=(s[b]=='{')-(s[b]=='}');b+=1
function=s[a:b]
code=r'''
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <future>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "bbport_threads.h"
using u32=std::uint32_t;using s64=std::int64_t;
thread_local sigjmp_buf* runtime_fault_recover=nullptr;
std::mutex mutex;std::condition_variable cv;
bool owner_ready=false;std::atomic<bool> block_dispatch{true};std::atomic<int> dispatches=0;
namespace Core {
struct Signals {
 static Signals* Instance(){static Signals s;return &s;}
 bool DispatchAccessViolation(void*,void*){
  ++dispatches;
  if(block_dispatch){std::unique_lock lk(mutex);cv.wait(lk,[]{return owner_ready;});}
  return true;
 }
};
}
FUNCTION
int main(){
 std::promise<int> recovered;auto done=recovered.get_future();
 std::thread worker([&]{
  BbThreads::background_thread=true;
  sigjmp_buf point;runtime_fault_recover=&point;
  int handled=bbgpu_handle_fault(nullptr,nullptr);
  runtime_fault_recover=nullptr;
  recovered.set_value(handled);
 });
 // The GPU owner waits for the worker's claim. A normal page-fault handler tries
 // to wait for this very same owner. Bound the test and release it on old code.
 bool stalled=done.wait_for(std::chrono::milliseconds(200))!=std::future_status::ready;
 {std::scoped_lock lk(mutex);owner_ready=true;}cv.notify_all();worker.join();
 if(stalled)return 2;
 assert(done.get()==0&&dispatches==0);
 block_dispatch=false;sigjmp_buf point;
 // Recovery on a real guest/GPU thread retains ordinary page tracking.
 runtime_fault_recover=&point;assert(bbgpu_handle_fault(nullptr,nullptr)==1);
 assert(dispatches==1);runtime_fault_recover=nullptr;
 // A helper without a speculative recovery point still gets real fault handling.
 BbThreads::background_thread=true;assert(bbgpu_handle_fault(nullptr,nullptr)==1);
 assert(dispatches==2);
 puts("PASS: speculative worker faults cannot wait on their GPU owner; ordinary tracked faults preserved");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)
 for old in (False,True):
  f=function
  if old:
   needle='    if (runtime_fault_recover && BbThreads::IsBackground()) return 0;'
   assert needle in f;f=f.replace(needle,'')
  (p/'test.cpp').write_text(code.replace('FUNCTION',f))
  subprocess.run(['g++','-std=c++20','-O2','-pthread','-Wall','-Wextra','-Werror','-I'+str(root/'gpu/shim'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  r=subprocess.run([str(p/'test')],timeout=5)
  assert r.returncode==(2 if old else 0),(old,r.returncode)
print('PASS: original dispatcher reproduces circular wait; patched production dispatcher eliminates it')
