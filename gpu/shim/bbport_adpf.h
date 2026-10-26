// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: Android Dynamic Performance Framework hints (APerformanceHint, API 33+) and a thermal
// readout (AThermal, API 30+). Both load libandroid.so with dlopen/dlsym, so the binary runs on any
// Android (min API 30) and the hints are silently off where the symbols are missing.
//
// One hint session holds the frame-critical threads: the game's main thread, the GPU command
// processor (shadPS4:GpuComm), bb:DrawRec, bb:VkRecorder (the first two workers) and the game's
// GXRenderThread. Threads join by their names (Common::SetCurrentThreadName, the runtime's guest
// thread names) as they start. Each submitted frame reports its CPU work duration; the target is
// 1 / the frame limit (BB_FPS_LIMIT; no limit: 1/60 s). BB_ADPF=0 turns all of it off (the thermal
// log too; BB_THERMAL_LOG=0 only the log).

#pragma once

namespace BbAdpf {
/// The calling thread is the game's main thread (the process's own thread, bb-probe).
void RegisterMainThread();
/// The calling thread got `name`: it joins the session when it is one of the frame-critical ones.
void NoteThreadName(const char* name);
/// A frame was submitted (flip): reports the CPU work duration since the last one.
void FrameSubmitted();
} // namespace BbAdpf
