"""Ensure the production download invokes the tested exclusion writer, without restore."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp').read_text()
s=s[s.index('void BufferCache::DownloadMemory('):s.index('// bbport: BB_BUFFER_STATS=1')]
assert 'BbReadbackMerge::ForEachGpuWrite(base,copy.size,cpu_owned' in s
assert 'cpu_owned.emplace_back(shadow.start + begin,shadow.start + end)' in s
assert 'guest_bytes' not in s and 'std::vector<u8>(guest' not in s
assert s.index('BbReadbackMerge::Normalize(cpu_owned)')<s.index('BbReadbackMerge::ForEachGpuWrite')
print('PASS: production readback excludes live CPU blocks; no stale snapshot restoration')
