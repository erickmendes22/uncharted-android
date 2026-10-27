"""Guard the user's requested release base and prohibit the previous readback override."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
for name, expected in json.loads((root / 'tests/release025-baseline.json').read_text()).items():
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == expected, name
for name in ('android/scripts/g2/game_app/GraphicsProfile.java',
             'gpu/shim/bbport_font_loader.cpp', 'android/GRAPHICS_RECOVERY_0.2.5.md'):
    assert not (root / name).exists(), name
helper = (root / 'android/scripts/g2/game_app/ArmCompatibility.java').read_text()
assert 'BB_GPU_MERGE=0' not in helper and 'BB_SIDE_DOWNLOAD=0' not in helper
activity = (root / 'android/scripts/g2/game_app/GameActivity.java').read_text()
assert 'configured=ArmCompatibility.cleanPerGame(configured)' in activity
buffer = (root / 'gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp').read_text()
start = buffer.index('bool BufferCache::DownloadOnSideQueue(')
end = buffer.index('BufferCache::~BufferCache()', start)
assert 'scheduler.Wait(' not in buffer[start:end], 'UMA path must not add producer waits'
patch = (root / 'android/scripts/port-arm64/patches/fex-0004-pause-hook-trap.patch').read_text()
assert 'Literal == 0x82' in patch and 'SyscallOp(Op, false)' in patch
assert '-  if (IsSyscallInst)' not in patch, 'Original SYSCALL semantics must be unchanged'
print('PASS: published release base, no S25 changes, no forced precise readback or new producer waits')
