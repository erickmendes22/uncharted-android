// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <cstdlib>
#include <boost/container/set.hpp>
#include <boost/container/small_vector.hpp>
#include "common/types.h"

namespace Serialization {
struct Archive;
}

namespace Shader {

using PFN_SrtWalker = void PS4_SYSV_ABI (*)(const u32* /*user_data*/, u32* /*flat_dst*/);
PFN_SrtWalker RegisterWalkerCode(const u8* ptr, size_t size);

#if defined(__aarch64__)
} // namespace Shader
extern "C" u64 guest_call(uintptr_t entry, const u64* args, unsigned count); // ../src/guest_cpu.h
namespace Shader {
// srt_interp.cpp: false when the walker uses an instruction it does not know.
bool RunSrtWalkerInterpreted(PFN_SrtWalker walker, const u32* user_data, u32* flat_dst);
#endif
// g2-arm64: walkers are x86-64 code (Xbyak); ARM64 hosts interpret them (srt_interp.cpp), or
// run them on the guest CPU (FEXCore) when the interpreter cannot (BB_SRT_FEX=1 forces that).
inline void RunSrtWalker(PFN_SrtWalker walker, const u32* user_data, u32* flat_dst) {
#if defined(__aarch64__)
    if (RunSrtWalkerInterpreted(walker, user_data, flat_dst)) {
        return;
    }
    const u64 args[2] = {reinterpret_cast<u64>(user_data), reinterpret_cast<u64>(flat_dst)};
    guest_call(reinterpret_cast<uintptr_t>(walker), args, 2);
#else
    walker(user_data, flat_dst);
#endif
}

// bbport: a ReadConst whose offset varies per lane (a constant block's pointer plus a per-pixel
// offset, as addr64 buffer loads compute it) reads a window of the block's first dwords that the
// walker copies to the flat buffer; its flags are the window's start with this bit.
constexpr u32 ReadConstWindowFlag = 1u << 16;
constexpr u32 ReadConstWindowDwords = 256;
// g2-arm64: bits 17-31 of such a ReadConst's flags: how many dwords the walker copies past the
// window, the largest immediate added to a per-lane offset (SplitWindowOffset, srt_window.h), or
// ReadConstWindowDefault: the shader's window did not fit, it has the default one, no split.
constexpr u32 ReadConstWindowExtraShift = 17;
constexpr u32 ReadConstWindowDefault = 0x7fff;
/// g2-arm64: a shader's window in dwords: ReadConstWindowDwords, or more for a shader that reads
/// further into its block (built in: the tiled light 43b8ee5e of U1/U2, whose light records take
/// ~28 KiB; BB_RC_WINDOW=<hash>:<dwords>,... adds or replaces entries).
u32 ReadConstWindowSize(u64 pgm_hash);

/// g2-arm64 diagnosis (BB_FLAT_PEEK=<hash>:<dword>,...): the values a dword of a shader's flat
/// buffer takes after its walks, counted and printed every 5 s.
inline bool FlatPeekEnabled() {
    static const bool enabled = std::getenv("BB_FLAT_PEEK") != nullptr;
    return enabled;
}
void NoteFlatBuf(u64 pgm_hash, const u32* flat, std::size_t dwords);

struct PersistentSrtInfo {
    // Special case when fetch shader uses step rates.
    struct SrtSharpReservation {
        u32 sgpr_base;
        u32 dword_offset;
        u32 num_dwords;
    };

    PFN_SrtWalker walker_func{};
    size_t walker_func_size{};
    u32 flattened_bufsize_dw = 16; // NumUserDataRegs

    void Serialize(Serialization::Archive& ar) const;
    bool Deserialize(Serialization::Archive& ar);
};

} // namespace Shader
