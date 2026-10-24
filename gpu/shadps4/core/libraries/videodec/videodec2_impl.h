// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "videodec2.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace Libraries::Videodec2 {

class VdecDecoder {
public:
    VdecDecoder(const OrbisVideodec2DecoderConfigInfo& configInfo,
                const OrbisVideodec2DecoderMemoryInfo& memoryInfo);
    ~VdecDecoder();

    s32 Decode(const OrbisVideodec2InputData& inputData, OrbisVideodec2FrameBuffer& frameBuffer,
               OrbisVideodec2OutputInfo& outputInfo);
    s32 Flush(OrbisVideodec2FrameBuffer& frameBuffer, OrbisVideodec2OutputInfo& outputInfo);
    s32 Reset();

private:
    AVFrame* ConvertNV12Frame(AVFrame& frame, s32 width, s32 height, s32 content_width,
                              s32 content_height);
    /// bbport: the size frames are handed to the game in (see OutputSize).
    void OutputSize(const AVFrame& frame, s32& width, s32& height, s32& content_width,
                    s32& content_height);

private:
    AVCodecContext* m_codec_context = nullptr;
    SwsContext* m_sws_context = nullptr;
    s32 m_max_width = 0;
    s32 m_max_height = 0;
    bool m_reported = false;
};

} // namespace Libraries::Videodec2