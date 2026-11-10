#!/usr/bin/env python3
"""Source-level integration assertions for PC v0.3.11-0.3.14 high-priority fixes.

Runtime behavior, GPU correctness, and actual FPS still need device validation.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def read(path):
    return (root/path).read_text()

shader=read("gpu/shadps4/shader_recompiler/ir/passes/effect_loop_clamp_pass.cpp")
hdr=read("gpu/shadps4/shader_recompiler/ir/passes/ir_passes.h")
recompiler=read("gpu/shadps4/shader_recompiler/recompiler.cpp")
ser=read("gpu/shadps4/video_core/renderer_vulkan/vk_pipeline_serialization.cpp")
for hash_value in ("0x8f067540","0x7d7e7206","0x0c936dd6","0x689d1af9"):
    assert hash_value in shader, hash_value
assert "BB_EFFECT_LOOP_CLAMP" in shader and "MaxEffectLoopCount = 4096" in shader
assert "MaxShadowTapCount = 256" in shader
assert "void EffectLoopClampPass(IR::Program& program);" in hdr
assert "Shader::Optimization::EffectLoopClampPass(program);" in recompiler
assert '#include "shader_recompiler/ir/passes/ir_passes.h"' in ser
assert "Shader::Optimization::EffectLoopClamp(pgm_hash)" in ser and '"_lc2"' in ser and '"_lc1"' in ser
assert "file(GLOB_RECURSE RECOMPILER" in read("gpu/CMakeLists.txt")

fixes=read("src/runtime_guest_fixes.c")
probe=read("src/probe.c")
assert "move_pool_256[]={0xb9,0x00,0x01,0x00,0x00}" in fixes
assert "ba 06 00 00 00 b9 40 00 00 00 41 b8 20 00 00 00" in fixes
assert "install_signature_fixes(segments, ns);" in probe
assert 'BB_ACTION_POOL_256' in probe
assert 'patch_job_descriptor_guard(image_end);' in probe
assert 'BB_JOB_GUARD' in probe
for game in ('"U1"','"U2"','"U3"'):
    assert game in probe[probe.index("static void patch_job_descriptor_guard("):probe.index("static void patch_job_lock_wait_us(")]
guard=probe[probe.index("static void patch_job_descriptor_guard("):probe.index("static void patch_job_lock_wait_us(")]
assert "find_unique_pattern(image_size,builds[b].loads)" in guard
assert "find_unique_pattern(image_size,builds[b].cave)" in guard
assert "cave-loads!=builds[b].distance" in guard
assert "memcpy(image+cave" in guard and "memcpy(image+loads" in guard

memory=read("src/runtime_memory.c")
for begin,end,output in [
    ("static ABI int32_t direct_map(", "static ABI int32_t direct_map_named(", "*out=where"),
    ("static ABI int32_t map_flexible(", "static ABI int32_t map_flexible_named(", "*inout=where"),
    ("static ABI int32_t query_protection(", "static ABI int32_t virtual_query(", "*start=(void *)run_start"),
    ("static ABI int32_t virtual_query(", "static ABI int32_t direct_memory_type(", "memcpy(info,&result,sizeof(result))"),
    ("static ABI int32_t direct_memory_type(", "static ABI int32_t batch_map2(", "*type=b.type"),
    ("static ABI int32_t reserve_range(", "static int32_t protect_locked(", "*inout=where"),
]:
    fn=memory[memory.index(begin):memory.index(end)]
    assert output in fn,(begin,output)
    assert fn.index("write_unlock();" if "map(" in begin or "map_flexible" in begin or "reserve_range" in begin else "read_unlock();") < fn.index(output), begin
batch=memory[memory.index("static ABI int32_t batch_map2("):memory.index("static ABI int32_t batch_map(")]
assert "if (count && !starts) return NO_MEMORY;" in batch
assert batch.index("write_unlock();") < batch.index("entries[i].start=starts[i]")

renderer=read("gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp")
rasterhdr=read("gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.h")
bufhdr=read("gpu/shadps4/video_core/buffer_cache/buffer_cache.h")
bufcpp=read("gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp")
assert "Copy64ShaderHash = 0x6a6422a9" in renderer
assert "void Rasterizer::NoteCopy64Readback(" in renderer
assert "NoteCopy64Readback(cs);" in renderer
assert "std::min<u64>({u64(CsRegs().dim_x) * 64" in renderer
assert "size > Copy64ReadbackMax" not in renderer  # size is named bytes
assert "bytes > Copy64ReadbackMax" in renderer
assert "u64(src.num_records), u64(dst.num_records)" in renderer
assert "COPY64EOP stats: events=%llu candidates=%llu async=%llu no_gpu=%llu" in renderer
assert "gated_events=%llu" in renderer
assert "event % 8192 == 0" in renderer
assert "buffer_cache.PendingGatedLanding()" in renderer
assert "sync_fallbacks=%llu sync_ms=%.3f pending_peak=%llu" in renderer
assert "max_pending.compare_exchange_weak" in renderer
assert "buffer_cache.PendingLandingCount()" in renderer
assert "u64 PendingLandingCount() const;" in bufhdr
assert "BufferCache::PendingLandingCount() const" in bufcpp
assert "static std::atomic<u64> sync_fallbacks" in renderer
assert "void Rasterizer::ReadBackSmallWrittenBuffers()" in renderer
assert "buffer_cache.DeferCopy64Readback(address, size)" in renderer
assert "if (captured) scheduler.Flush();" in renderer
assert "copy64_readbacks;" in rasterhdr
assert "bool DeferCopy64Readback(VAddr address, u64 size);" in bufhdr
assert "QueueGuestSnapshot(address, address, size, false)" in bufcpp

options=read("android/scripts/g2/game_app/RendererOptions.java")
launcher=read("android/scripts/g2/game_app/Launcher.java")
activity=read("android/scripts/g2/game_app/GameActivity.java")
assert 'compatibilitySwitch(root,"Small GPU result readbacks (U2/U3)","gpu_small_readbacks",false)' in launcher
assert 'options.getBoolean("gpu_small_readbacks",false)' in activity
assert '"u2".equals(game)||"u3".equals(game)' in options
assert 'BB_READBACK_SMALL_BUFFERS="+(enabled?"1":"0")' in options
assert 'Os.setenv("BB_COPY64_EOP_READBACK","u3".equals(game) &&' in activity
assert 'options.getBoolean("u3_raycaster_readback",false)' in activity
assert 'options.getBoolean("gpu_shader_loop_safety",true)' in activity
print("PASS: six high-priority upstream integration paths wired; optional broad readbacks off by default")
