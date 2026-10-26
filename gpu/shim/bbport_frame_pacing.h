// SPDX-License-Identifier: GPL-2.0-or-later
// uncharted-android v0.2.5 / uncharted_Linux d976ca3: bounded frame slots.
#pragma once
#include <algorithm>
#include <chrono>
namespace BbFramePacing {
template<class Time,class Duration>
Time Next(Time previous,Time now,Duration period){
    return std::min(std::max(previous+period,now+period-period/16),now+period);
}
}
