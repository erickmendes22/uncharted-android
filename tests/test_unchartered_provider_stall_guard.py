#!/usr/bin/env python3
"""Source policy gate: provider retries and Vulkan presentation waits remain safe.

These tests validate source integration, not a running GPU or recovered gameplay.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
up=(root/"gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.cpp").read_text()
pr=(root/"gpu/shadps4/video_core/renderer_vulkan/vk_presenter.cpp").read_text()
start=up.index("bool TemporalUpscaler::OnFrameStart()")
end=up.index("void TemporalUpscaler::OnDispatch(",start)
frame=up[start:end]
assert "applied_upscaler != upscaler ||" in frame
assert "upscaler == BbSettings::UpscalerOff) failed = false;" in frame
assert frame.index("failed = false;")<frame.index("const bool active = Active();")
assert "display_redirect = ui_phase && !movie_bypass && !changed;" in frame
assert 'upscaler == BbSettings::UpscalerFsr3 ? "FSR 3.1"' in frame
assert 'upscaler == BbSettings::UpscalerGsr2 ? "Snapdragon GSR 2"' in frame
first=pr[pr.index("Frame* Presenter::PrepareLastFrame()"):pr.index("Frame* Presenter::GetRenderFrame()")]
second=pr[pr.index("Frame* Presenter::GetRenderFrame()"):pr.index("void Presenter::SetExpectedGameSize(")]
assert "1'000'000'000ull" in first and "1'000'000'000ull" in second
assert "fence_timeout_count % 5 == 0" in first and "fence_timeout_count % 5 == 0" in second
assert "Scheduler::ReportStall();" in first and second.count("Scheduler::ReportStall();")>=2
assert "free_cv.wait_for(lock, std::chrono::seconds(5)" in second
assert "lock.unlock();" in second and "lock.lock();" in second
assert "waitForFences(frame->present_done, false, std::numeric_limits<u64>::max())" not in first+second
print("PASS: retry clears provider failure; stale output avoided; presenter waits report GPU stalls")
