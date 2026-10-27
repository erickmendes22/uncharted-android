#!/usr/bin/env python3
"""Compile the production landing worker/gates with a deterministic fake GPU."""
import pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp').read_text()
h=(root/'gpu/shadps4/video_core/buffer_cache/buffer_cache.h').read_text()
def function(name):
 a=s.index('void BufferCache::'+name+'(') if name not in ('PendingLanding','LandingGateActive') else s.index(('u64' if name=='PendingLanding' else 'bool')+' BufferCache::'+name+'(')
 brace=s.index('{',a);depth=1;b=brace+1
 while depth:
  if s[b]=='{':depth+=1
  elif s[b]=='}':depth-=1
  b+=1
 return s[a:b]
fields=h[h.index('    struct GatedLabel {'):h.index('    std::unique_ptr<FaultManager> fault_manager;')]
# Keep the exact synchronization fields; exclude unrelated mapping helpers.
fields=fields[:fields.index('    // RedVorteX ARM readbacks:')].replace('    bool ExecuteLanding(Landing& landing);', '')
code=r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
using u64=unsigned long long; using u32=unsigned;
namespace Common { void SetCurrentThreadName(const char*) {} }
class BufferCache {
public:
 struct Landing { u64 seq; };
 FIELDS
 std::mutex gpu_mutex;
 std::condition_variable gpu_cv;
 u64 permitted=0;
 std::promise<void> first_started, second_started;
 bool ExecuteLanding(Landing& l) {
   if(l.seq==1)first_started.set_value();
   if(l.seq==2)second_started.set_value();
   std::unique_lock lk(gpu_mutex);
   gpu_cv.wait(lk,[&]{return permitted>=l.seq;});
   return true;
 }
 bool LandingGateActive() const;
 u64 PendingLanding() const;
 void WaitLanding(u64);
 void RunAfterLanding(u64,std::function<void()>);
 void add() {
   std::scoped_lock lk(landing_mutex);
   auto item=std::make_shared<Landing>();item->seq=++landing_registered_seq;
   landing_registered.store(item->seq,std::memory_order_release);
   landing_queue.push_back(item);landing_cv.notify_one();
 }
 void allow(u64 n) {std::scoped_lock lk(gpu_mutex);permitted=n;gpu_cv.notify_all();}
 ~BufferCache(){
   allow(100);
   {std::scoped_lock lk(landing_mutex);landing_stop=true;}
   landing_cv.notify_all();landing_done_cv.notify_all();
   if(landing_thread.joinable())landing_thread.join();
 }
};
FUNCTIONS
int main(){
 BufferCache b;
 b.add();b.add();b.landing_thread=std::thread([&]{b.LandingWorker();});
 b.first_started.get_future().get();
 assert(b.PendingLanding()==2);
 std::atomic<int> order=0;
 std::promise<void> first_label,second_label;
 b.RunAfterLanding(1,[&]{assert(b.landing_done.load()>=1);assert(order++==0);});
 b.RunAfterLanding(1,[&]{assert(order++==1);first_label.set_value();});
 b.RunAfterLanding(2,[&]{assert(b.landing_done.load()>=2);assert(order++==2);second_label.set_value();});
 assert(order==0); // No completion label while GPU output is unavailable.
 b.allow(1);
 first_label.get_future().get();
 b.second_started.get_future().get();
 assert(order==2); // Earlier labels do not wait for later GPU work.
 assert(b.LandingGateActive());
 b.allow(2);second_label.get_future().get();b.WaitLanding(2);
 assert(order==3);
 std::promise<void> final;
 b.RunAfterLanding(0,[&]{assert(order++==3);final.set_value();});
 final.get_future().get();
 // Shutdown releases direct label waits even when the requested sequence is absent.
 auto waiter=std::async(std::launch::async,[&]{b.WaitLanding(3);});
 {std::scoped_lock lk(b.landing_mutex);b.landing_stop=true;}
 b.landing_done_cv.notify_all();b.landing_cv.notify_all();
 waiter.get();
 puts("PASS: actual landing worker preserves data-before-label, label order, overlap and cancellation");
}
'''.replace('FIELDS',fields).replace('FUNCTIONS','\n'.join(function(n) for n in ['LandingWorker','PendingLanding','LandingGateActive','WaitLanding','RunAfterLanding']))
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=10)
