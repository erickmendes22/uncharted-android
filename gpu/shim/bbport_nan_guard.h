// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: the NaN guard on colour outputs, shared by the shader recompiler and the shader cache.
#pragma once

#include <cstdlib>
#include "bbport_renderer_options.h"

namespace BbNanGuard {

/// Colour outputs to float render targets: NaN becomes 0, so one bad pixel cannot grow into a
/// black 16x16 block through the games' tiled post-processing (Uncharted 2 on the Steam Deck).
/// BB_NAN_GUARD: 1 (default) NaN only, 2 also clamps +-Inf to the half-float range (that broke
/// Uncharted 1's lighting and fog: blue trees), 0 off.
inline int NanGuardMode() {
    static const int mode = [] {
        const char* env = std::getenv("BB_NAN_GUARD");
        return env && (env[0] == '0' || env[0] == '2') ? env[0] - '0' : 1;
    }();
    return mode;
}

inline bool ExportNanGuard() {
    return NanGuardMode() != 0;
}

/// g2-arm64: GCN's output clamp turns NaN into 0 (like D3D's saturate); SPIR-V's FClamp leaves it
/// undefined, Adreno keeps the NaN and the guard above shows it black: Uncharted 1's statue moss,
/// clamp(exp2(power * log2(dot))) with a zero gloss power where the dot is 0. Fragment shaders get
/// NaN -> 0; BB_SATURATE_NAN_FS=0 turns it off (vertex shaders: upstream's BB_SATURATE_NAN).
inline bool SaturateNanFragment() {
    static const bool enabled = [] {
        const char* env = std::getenv("BB_SATURATE_NAN_FS");
        return !(env && env[0] == '0');
    }();
    return enabled;
}

/// Shaders compiled in each mode are cached apart (the default modes in the plain cache).
inline const char* ShaderCacheSuffix() {
    // Versioned for the new compiler and segregated for every comparison setting.
    static const std::string suffix = std::string{"-r6-nan"} + std::to_string(NanGuardMode()) +
        "-sat" + (SaturateNanFragment() ? "1" : "0") +
        "-lights" + (BbRenderer::FullLights() ? "128" : "32") +
        "-exec" + (BbRenderer::ScalarExec() ? "1" : "0");
    return suffix.c_str();
}

} // namespace BbNanGuard
