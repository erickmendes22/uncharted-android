// SPDX-FileCopyrightText: Copyright 2026 uncharted-g2
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>
#include "common/types.h"
#include "shader_recompiler/ir/value.h"

namespace Shader {

/// g2-arm64: a windowed ReadConst's offset (ReadConstWindowFlag) as its per-lane part and the
/// immediates added to it, when their sum cannot wrap (the per-lane part is a logical right shift,
/// below 2^31): the window's clamp then goes on the per-lane part alone, the dwords of one load
/// keep consecutive offsets from one base and Turnip merges them into vector loads (the light
/// shader 43b8ee5e: 63 -> 24 loads, 914 -> 736 instructions). Otherwise {offset, 0}.
std::pair<IR::Value, u32> SplitWindowOffset(const IR::Value& offset);

} // namespace Shader
