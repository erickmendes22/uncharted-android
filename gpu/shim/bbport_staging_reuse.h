// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include <algorithm>

namespace BbStaging {
// Caller serializes access and returns a buffer only after GPU completion and
// the CPU copy. A size miss retires one free buffer before allocating another.
// Together with the 16-pending limit this bounds active + idle buffers to 16.
template<class Buffer, std::size_t Capacity = 16>
class Reuse {
    std::vector<std::unique_ptr<Buffer>> free;
public:
    std::unique_ptr<Buffer> Take(std::size_t bytes) {
        auto best = free.end();
        for (auto it=free.begin();it!=free.end();++it)
            if ((*it)->size_bytes>=bytes && (best==free.end() || (*it)->size_bytes<(*best)->size_bytes)) best=it;
        if(best==free.end()) {
            if(!free.empty())free.pop_back();
            return {};
        }
        auto result=std::move(*best);free.erase(best);return result;
    }
    void Put(std::unique_ptr<Buffer> buffer) {
        if(buffer && free.size()<Capacity)free.push_back(std::move(buffer));
    }
};
}
