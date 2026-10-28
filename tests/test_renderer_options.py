#!/usr/bin/env python3
"""Exercise the actual cache-key and driver-header policies, each mode in a fresh process."""
from pathlib import Path
import os, subprocess, tempfile, itertools
root=Path(__file__).resolve().parents[1]
code=r'''
#include "bbport_nan_guard.h"
#include "bbport_driver_cache.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <limits>
int main(){
 std::array<std::uint8_t,16> uuid{};uuid[15]=42;
 std::vector<char> data(64);
 std::array<std::uint32_t,4> h{32,1,0x5143,0x800};
 std::memcpy(data.data(),h.data(),16);std::memcpy(data.data()+16,uuid.data(),16);
 assert(BbDriverCache::Matches(data,0x5143,0x800,uuid));
 for(size_t n=0;n<32;n++)assert(!BbDriverCache::Matches(std::span{data.data(),n},0x5143,0x800,uuid));
 assert(!BbDriverCache::Matches(data,1,0x800,uuid));
 assert(!BbDriverCache::Matches(data,0x5143,1,uuid));
 uuid[0]=1;assert(!BbDriverCache::Matches(data,0x5143,0x800,uuid));uuid[0]=0;
 for(auto bad: {0u,31u,65u,std::numeric_limits<unsigned>::max()}){
  h[0]=bad;std::memcpy(data.data(),h.data(),16);
  assert(!BbDriverCache::Matches(data,0x5143,0x800,uuid));
 }
 h[0]=32;h[1]=2;std::memcpy(data.data(),h.data(),16);
 assert(!BbDriverCache::Matches(data,0x5143,0x800,uuid));
 puts(BbNanGuard::ShaderCacheSuffix());
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I'+str(root/'gpu/shim'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 keys=['BB_NAN_GUARD','BB_SATURATE_NAN_FS','BB_FULL_LIGHT_TABLE','BB_SCALAR_EXEC_FIX']
 seen=set()
 for values in itertools.product(['0','1','2'],['0','1'],['0','1'],['0','1']):
  env=os.environ.copy();env.update(zip(keys,values))
  key=subprocess.check_output([str(p/'test')],env=env,text=True).strip()
  assert key not in seen,(values,key)
  seen.add(key)
 assert len(seen)==24
print('PASS: invalid driver blobs rejected before Vulkan; all 24 graphics modes have distinct cache keys')
