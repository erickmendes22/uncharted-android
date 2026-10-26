// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 lsfg-vk
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Uncharted ARM port: the host side of the frame generation code ported from Eden (eden-emu PR
// #4263) by way of ARMSX2 (ARMSX2/ARMSX2 90126bd, pcsx2/GS/Renderers/Vulkan/FrameGen). In ARMSX2
// this file mapped Eden's scalar aliases and settings onto PCSX2; here they map onto the port:
// the settings come from LsfgConfig (vk_frame_generation.cpp fills it from BbSettings) and the
// log goes to stdout, the game log. See README.md in this folder.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s32 = std::int32_t;
using f32 = float;
using f64 = double;

namespace VideoCore::FrameGen {
/// Hard ceiling on interpolated frames per rendered frame. Matches LSFG_MAX_GENERATIONS in the
/// pass code: the shader set only carries weights for three.
inline constexpr size_t MAX_GENERATIONS = 3;
} // namespace VideoCore::FrameGen

/// What ARMSX2 read from GSConfig.Lsfg*: set by the port before frame generation runs.
struct LsfgConfiguration {
    bool enabled = false;
    u32 multiplier = 2;      ///< frames displayed per rendered frame: 2 = one interpolated
    std::string dll_path;    ///< the player's own Lossless.dll
    bool fp16 = false;       ///< half-precision shaders when the device has them
    u8 flow_scale = 100;     ///< motion resolution in percent; 100 = automatic
    u16 target_rate = 0;     ///< adaptive pacer's output rate in Hz; 0 = fixed multiplier
};
inline LsfgConfiguration& LsfgConfig() {
    static LsfgConfiguration config;
    return config;
}

/// ARMSX2's Console.WriteLn / Warning / Error, onto the game log.
struct LsfgConsole {
    template <typename... Args>
    void WriteLn(const char* format, Args... args) const {
        Print("", format, args...);
    }
    template <typename... Args>
    void Warning(const char* format, Args... args) const {
        Print("warning: ", format, args...);
    }
    template <typename... Args>
    void Error(const char* format, Args... args) const {
        Print("error: ", format, args...);
    }

private:
    template <typename... Args>
    static void Print(const char* kind, const char* format, Args... args) {
        std::printf("%s", kind);
        if constexpr (sizeof...(Args) == 0) {
            std::printf("%s", format);
        } else {
            std::printf(format, args...);
        }
        std::printf("\n");
    }
};
inline constexpr LsfgConsole Console{};

namespace Vulkan::FrameGenSettings {
/// Interpolated frames per rendered frame. The setting is the MULTIPLIER (2 = one interpolated
/// frame); the ported code counts GENERATIONS.
inline size_t Generations() {
    const u32 mult = std::max<u32>(LsfgConfig().multiplier, 2u);
    return std::min<size_t>(mult - 1u, VideoCore::FrameGen::MAX_GENERATIONS);
}
/// Upper bound the pacer may probe up to.
inline size_t MaxGenerations() {
    return LsfgConfig().enabled ? Generations() : 0;
}
/// Target output rate in Hz, or 0 to hold the multiplier fixed.
inline f32 TargetRate() {
    return static_cast<f32>(LsfgConfig().target_rate);
}
} // namespace Vulkan::FrameGenSettings
