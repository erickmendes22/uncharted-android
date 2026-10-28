// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdlib>
#include <string>

// Startup-only comparison settings. GameActivity reapplies them on every boot/loadexec.
namespace BbRenderer {
inline bool Flag(const char* key, bool fallback) {
    const char* v = std::getenv(key);
    return v ? v[0] == '1' : fallback;
}
inline bool DriverCache() {
    static const bool v = Flag("BB_DRIVER_CACHE", true); return v;
}
inline bool WorkerCompile() {
    static const bool v = Flag("BB_PREP_COMPILE", true); return v;
}
inline bool BackgroundWarmup() {
    static const bool v = Flag("BB_BACKGROUND_WARMUP", true); return v;
}
inline bool FullLights() {
    static const bool v = Flag("BB_FULL_LIGHT_TABLE", true); return v;
}
inline bool ScalarExec() {
    static const bool v = Flag("BB_SCALAR_EXEC_FIX", false); return v;
}
inline bool SmallReadbacks() {
    static const bool v = Flag("BB_READBACK_SMALL_BUFFERS", false); return v;
}
}
