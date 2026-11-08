#!/usr/bin/env python3
"""Regression gate: 0.2.9 pre-worker base + bounded RedVorteX v0.1.8 buffers.

The 0.3.0 guest job-worker sleep/spin/wake port is specifically excluded after
the U2 1.02 performance regression. The 0.1.8 WriteData fast path is separately
gated; no on-device FPS claims are made from these static checks.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
def read(path): return (root/path).read_text()
probe=read("src/probe.c")
compat=read("android/scripts/g2/game_app/ArmCompatibility.java")
liverpool=read("gpu/shadps4/video_core/amdgpu/liverpool.cpp")
buffer=read("gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp")
ring=read("gpu/shadps4/video_core/renderer_vulkan/vk_constant_ring.h")

# Exact pre-regression guest job-worker implementation, not a switch hiding
# the 0.3.0 backport.
assert "patch_job_sleep(" not in probe
assert "BB_JOB_SLEEP=" not in compat
assert "BB_JOB_SPINS=" not in compat
assert "BB_JOB_RESUME_WAKE=" not in compat
assert "runtime_job_spin_install" in read("src/runtime_jobspin.c")

# Restore mobile buffer budgets sourced from RedVorteX v0.1.8 without
# changing desktop constants or the 0.2.9 game's binary patches.
assert "static constexpr size_t STREAM_BUFFER_SIZE = 256_MB;" in buffer
assert "static constexpr size_t STREAM_BUFFER_SIZE = 128_MB;" in buffer
assert "static constexpr u64 Capacity = 128ull << 20;" in ring
assert "static constexpr u64 Capacity = 32ull << 20;" in ring
assert "RunWriteDataSeq" in liverpool and "FenceWritten(task.seq)" in liverpool
# RedVorteX 0.1.8 label fast path is OFF by default and only opt-in.
assert "BB_CHAIN_WRITEDATA" in liverpool
assert "TryChainWriteDataSeq(rasterizer, task)" in liverpool
assert "FenceWritten(seq); // only after the guest bytes land" in liverpool
assert "VideoCore::ChainedWrites::Landed(landed)" in liverpool
assert "VideoCore::ChainedWrites::AllowWait()" in liverpool
assert "IsRegionCpuModified(address, size)" in liverpool
assert "IsRegionGpuModified(address, size)" in liverpool
assert "VideoCore::ChainedWrites::Overlaps" in read("gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp")
assert "ChainedWrites::MustWait(device_addr, size)" in buffer
assert "ChainedWrites::MustWait(image.info.guest_address, image.info.guest_size)" in read("gpu/shadps4/video_core/texture_cache/texture_cache.cpp")
assert 'options.getBoolean("arm_chain_writedata",false)' in read("android/scripts/g2/game_app/GameActivity.java")
assert 'compatibilitySwitch(root,"Async GPU label writes (experimental)","arm_chain_writedata",false)' in read("android/scripts/g2/game_app/Launcher.java")
assert '"BB_CHAIN_WRITEDATA"' in compat
assert "BB_JOB_SLEEP" not in probe
print("PASS: rollback + RedVorteX buffers; optional sequenced WriteData dependencies")


