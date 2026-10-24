#!/usr/bin/env bash
# heapprof.sh <secs> <out.pftrace> [sampling bytes]: a native heap profile (heapprofd) of the running
# game, symbolized, and the call stacks holding the most unreleased memory at the end.
#   bash android/scripts/g2/heapprof.sh 300 /tmp/heap.pftrace
# Needs: the game running (the APK is profileable from the shell), WSL with the perfetto tools in
# ~/src/perfetto-tools (curl -sSL -o trace_processor https://get.perfetto.dev/trace_processor, the
# same for traceconv), the installed build's unstripped libbbgpu.so/libmain.so in
# ~/src/uncharted-android-build (their build IDs match the stripped ones in the APK), other libraries
# (libc++_shared, SDL3, FFmpeg) in ~/src/symbols, llvm-symbolizer from the NDK.
# Found 2026-10-21: the compute queue's nested indirect-buffer coroutines leaking ~32 KiB each.
set -euo pipefail
export MSYS_NO_PATHCONV=1
SECS=$1; OUT=$2; SAMPLING=${3:-32768}
ADB=$(ls -d /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Google.PlatformTools_*/platform-tools | head -1)/adb.exe
"$ADB" shell pidof dev.unchartedg2.game > /dev/null || { echo "the game is not running" >&2; exit 1; }
CFG=$(mktemp)
cat > "$CFG" <<EOF
buffers { size_kb: 131072 fill_policy: DISCARD }
data_sources {
  config {
    name: "android.heapprofd"
    heapprofd_config {
      sampling_interval_bytes: $SAMPLING
      process_cmdline: "dev.unchartedg2.game"
      shmem_size_bytes: 16777216
      block_client: false
    }
  }
}
duration_ms: $((SECS * 1000))
EOF
"$ADB" push "$(cygpath -w "$CFG")" /data/local/tmp/heapprof.cfg > /dev/null
rm -f "$CFG"
"$ADB" shell "rm -f /data/misc/perfetto-traces/heapprof.pftrace; cat /data/local/tmp/heapprof.cfg | perfetto --txt -c - -o /data/misc/perfetto-traces/heapprof.pftrace" 2>&1 | tail -1
"$ADB" pull /data/misc/perfetto-traces/heapprof.pftrace "$(cygpath -w "$OUT")" > /dev/null
# the trace as a WSL path (C:/x -> /mnt/c/x)
W=/mnt/$(cygpath -m "$OUT" | sed -E 's#^([A-Za-z]):#\L\1#')
wsl -d Ubuntu-24.04 -e bash -c "set -e; cd ~/src/perfetto-tools
  export PATH=\$HOME/android/android-ndk-r30/toolchains/llvm/prebuilt/linux-x86_64/bin:\$PATH
  cp '$W' heap.pftrace
  PERFETTO_BINARY_PATH=\$HOME/src/uncharted-android-build:\$HOME/src/symbols python3 -I ./traceconv symbolize heap.pftrace heap.symbols > /dev/null 2>&1 || true
  cat heap.pftrace heap.symbols > heap.sym.pftrace 2>/dev/null || cp heap.pftrace heap.sym.pftrace
  cat > heap.sql <<'SQL'
create perfetto table top as
select callsite_id, sum(size) as bytes, sum(count) as n from heap_profile_allocation
group by callsite_id having bytes > 0 order by bytes desc limit 8;
with recursive stack(id, frame_id, parent_id, depth, root) as (
  select c.id, c.frame_id, c.parent_id, c.depth, c.id from stack_profile_callsite c
  where c.id in (select callsite_id from top)
  union all
  select p.id, p.frame_id, p.parent_id, p.depth, s.root from stack_profile_callsite p join stack s on p.id = s.parent_id
)
select (select bytes from top where callsite_id = s.root) as bytes, s.root, s.depth,
       substr(coalesce(sym.name, f.name, printf('0x%x', f.rel_pc)), 1, 100) as frame
from stack s join stack_profile_frame f on f.id = s.frame_id
left join stack_profile_symbol sym on sym.symbol_set_id = f.symbol_set_id
order by bytes desc, s.root, s.depth desc;
SQL
  python3 -I ./trace_processor -q heap.sql heap.sym.pftrace 2>/dev/null"
