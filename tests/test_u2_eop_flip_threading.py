#!/usr/bin/env python3
"""Regression guard: EOP flip handlers must never synchronously wait for VkRecorder."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def source(relative):
    return (ROOT / relative).read_text()


driver = source("gpu/shadps4/core/libraries/videoout/driver.cpp")
liverpool = source("gpu/shadps4/video_core/amdgpu/liverpool.cpp")
scheduler_h = source("gpu/shadps4/video_core/renderer_vulkan/vk_scheduler.h")
scheduler = source("gpu/shadps4/video_core/renderer_vulkan/vk_scheduler.cpp")
rasterizer = source("gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.h")
cache_h = source("gpu/shadps4/video_core/buffer_cache/buffer_cache.h")
cache = source("gpu/shadps4/video_core/buffer_cache/buffer_cache.cpp")
gate = source("gpu/shadps4/video_core/landing_gate.h")

assert "static bool OnRecorderThread();" in scheduler_h
assert "on_recorder_thread = true;" in scheduler
assert "return on_recorder_thread;" in scheduler
assert "else if (Vulkan::Scheduler::OnRecorderThread())" in driver
queued = driver.split("else if (Vulkan::Scheduler::OnRecorderThread())", 1)[1].split(
    "} else {", 1
)[0]
assert "liverpool->SendCommand" in queued
assert queued.find("SubmitFlipInternal(") < queued.find("eop_flip_reached()")

assert "bool off_recorder = false" in scheduler_h
assert "off_recorder" in rasterizer
assert "landing_seq, true); // always run flip interrupt" in liverpool
assert "LandingGate::Post(landing_seq, std::move(fire))" in scheduler
assert "LandingGate::Run(landing_seq, std::move(fire))" in scheduler

assert "void Post(u64 seq, std::function<void()> fn);" in gate
assert "bool off_caller = false" in cache_h
assert "RunAfterLanding(seq, std::move(fn), true)" in cache
assert "ready && !off_caller" in cache
assert re.search(r"void Post\(u64 seq, std::function<void\(\)> fn\)", cache)

print("PASS: EOP flip interrupts are off Vulkan recorder; fallback EOP submission and landing ordering guarded")
