// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: Android Dynamic Performance Framework hints and a thermal readout (bbport_adpf.h).
//
// Work duration of a frame: the time the game's main thread and the GPU command processor actually
// wanted the CPU between two submitted frames, from /proc/self/task/<tid>/schedstat (running time
// plus run-queue wait: a thread descheduled on a busy phone is the case the hint is for; sleeping
// in the frame limiter, on a vblank or on a semaphore is neither), else from the thread's CPU
// clock. The longer of the two is reported: they work in parallel. The recorder threads spin while
// they wait for work, so their CPU time says nothing about the frame and is not measured (they are
// in the session, which is what the hint's boost applies to).
#include "bbport_adpf.h"
#include "core/emulator_settings.h"

#include <dlfcn.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace BbAdpf {
namespace {
using u64 = std::uint64_t;
using i64 = std::int64_t;

struct HintManager;
struct HintSession;
struct ThermalManager;

struct Api {
    HintManager* (*get_manager)() = nullptr;
    HintSession* (*create_session)(HintManager*, const std::int32_t*, std::size_t, i64) = nullptr;
    int (*update_target)(HintSession*, i64) = nullptr;
    int (*report_actual)(HintSession*, i64) = nullptr;
    void (*close_session)(HintSession*) = nullptr;
    int (*set_threads)(HintSession*, const std::int32_t*, std::size_t) = nullptr;           // API 34
    int (*set_prefer_power_efficiency)(HintSession*, bool) = nullptr;                       // API 35
    ThermalManager* (*thermal_acquire)() = nullptr;                                         // API 30
    int (*thermal_status)(ThermalManager*) = nullptr;                                       // API 30
    float (*thermal_headroom)(ThermalManager*, int) = nullptr;                              // API 31
};

bool EnvOff(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] == '0';
}

/// A thread whose scheduling time is measured per frame.
struct Probe {
    int tid = -1;
    int fd = -1;
    bool valid = false;
    u64 last = 0; ///< running + waiting time at the last frame, ns
};

enum class Kind { Main, GpuComm, DrawRec, Recorder, RenderThread };

struct State {
    std::mutex lock;
    bool enabled = false;
    bool failed = false;     ///< the session cannot be made: off for good
    std::atomic<bool> dlopen_done{false};
    Api api;
    HintManager* manager = nullptr;
    HintSession* session = nullptr;
    std::vector<std::int32_t> tids;
    std::size_t session_threads = 0; ///< threads the session has now
    std::atomic<bool> dirty{false};  ///< a thread joined since the session was made
    Probe main_probe, gpu_probe;
    int recorders = 0;
    bool have_main = false, have_gpu = false, have_draw = false, have_render = false;
    i64 target_ns = 0;
    u64 last_frame_ns = 0;
    int attempts = 0;
    u64 next_attempt_ns = 0;
    bool measure_logged = false;
    std::once_flag once;
};
State& S() {
    static State state;
    return state;
}

u64 NowNs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return u64(ts.tv_sec) * 1000000000ull + u64(ts.tv_nsec);
}

/// libandroid's symbols, once. Without APerformanceHint (API < 33) the hints stay off; the thermal
/// readout needs only AThermal (API 30).
void LoadApi() {
    State& s = S();
    s.enabled = !EnvOff("BB_ADPF");
    if (!s.enabled) {
        return;
    }
    void* lib = dlopen("libandroid.so", RTLD_NOW);
    if (!lib) {
        s.enabled = false;
        return;
    }
    const auto sym = [&](const char* name) { return dlsym(lib, name); };
    Api& a = s.api;
    a.get_manager = reinterpret_cast<decltype(a.get_manager)>(sym("APerformanceHint_getManager"));
    a.create_session =
        reinterpret_cast<decltype(a.create_session)>(sym("APerformanceHint_createSession"));
    a.update_target = reinterpret_cast<decltype(a.update_target)>(
        sym("APerformanceHint_updateTargetWorkDuration"));
    a.report_actual = reinterpret_cast<decltype(a.report_actual)>(
        sym("APerformanceHint_reportActualWorkDuration"));
    a.close_session =
        reinterpret_cast<decltype(a.close_session)>(sym("APerformanceHint_closeSession"));
    a.set_threads = reinterpret_cast<decltype(a.set_threads)>(sym("APerformanceHint_setThreads"));
    a.set_prefer_power_efficiency = reinterpret_cast<decltype(a.set_prefer_power_efficiency)>(
        sym("APerformanceHint_setPreferPowerEfficiency"));
    a.thermal_acquire =
        reinterpret_cast<decltype(a.thermal_acquire)>(sym("AThermal_acquireManager"));
    a.thermal_status =
        reinterpret_cast<decltype(a.thermal_status)>(sym("AThermal_getCurrentThermalStatus"));
    a.thermal_headroom =
        reinterpret_cast<decltype(a.thermal_headroom)>(sym("AThermal_getThermalHeadroom"));
    s.dlopen_done.store(true, std::memory_order_release);
}

void Init() {
    std::call_once(S().once, LoadApi);
}

bool HintsAvailable(const Api& a) {
    return a.get_manager && a.create_session && a.update_target && a.report_actual &&
           a.close_session;
}

i64 TargetNs() {
    const unsigned limit = EmulatorSettings.GetFrameLimit();
    return 1000000000ll / (limit ? limit : std::max(BbDisplayRefreshHz(),60u));
}

void OpenProbe(Probe& p, int tid) {
    p.tid = tid;
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/self/task/%d/schedstat", tid);
    p.fd = open(path, O_RDONLY | O_CLOEXEC);
    p.valid = false;
}

/// The thread's running + run-queue waiting time, ns; false when it cannot be read.
bool Sample(Probe& p, u64& total) {
    if (p.tid < 0) {
        return false;
    }
    if (p.fd >= 0) {
        char buf[96];
        const ssize_t n = pread(p.fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = 0;
            char* end = nullptr;
            const u64 run = std::strtoull(buf, &end, 10);
            const u64 wait = end ? std::strtoull(end, nullptr, 10) : 0;
            total = run + wait;
            return true;
        }
        close(p.fd);
        p.fd = -1;
    }
    // CPU-time clock of the thread: the clock id the kernel gives (~tid << 3 | SCHED | PERTHREAD).
    timespec ts;
    const clockid_t clock = clockid_t((~static_cast<unsigned>(p.tid)) << 3) | 6;
    if (clock_gettime(clock, &ts) == 0) {
        total = u64(ts.tv_sec) * 1000000000ull + u64(ts.tv_nsec);
        return true;
    }
    p.tid = -1;
    return false;
}

/// The session with every thread known now (State::lock held): made, or its threads replaced.
void EnsureSession() {
    State& s = S();
    if (s.failed || !s.enabled || s.tids.empty()) {
        return;
    }
    const u64 now = NowNs();
    if (!s.session) {
        if (now < s.next_attempt_ns) {
            return;
        }
        s.next_attempt_ns = now + 5000000000ull;
        ++s.attempts;
        s.target_ns = TargetNs();
        const char* why = nullptr;
        if (!HintsAvailable(s.api)) {
            why = "no APerformanceHint in libandroid.so (Android below 13)";
            s.attempts = 99;
        } else {
            if (!s.manager) {
                s.manager = s.api.get_manager();
            }
            if (!s.manager) {
                why = "APerformanceHint_getManager returned null";
            } else if (!(s.session = s.api.create_session(s.manager, s.tids.data(), s.tids.size(),
                                                           s.target_ns))) {
                why = "APerformanceHint_createSession returned null";
            }
        }
        if (!s.session) {
            if (s.attempts >= 3) {
                s.failed = true;
                std::printf("ADPF: off (%s)\n", why);
            }
            return;
        }
        s.session_threads = s.tids.size();
        if (s.api.set_prefer_power_efficiency) {
            s.api.set_prefer_power_efficiency(s.session, false);
        }
        std::printf("ADPF: session with %zu threads, target %.1f ms\n", s.session_threads,
                    double(s.target_ns) / 1e6);
        return;
    }
    // A thread joined later: replace the session's threads (API 34), else make the session again.
    if (s.tids.size() == s.session_threads) {
        return;
    }
    if (s.api.set_threads) {
        if (s.api.set_threads(s.session, s.tids.data(), s.tids.size()) != 0) {
            std::printf("ADPF: APerformanceHint_setThreads failed with %zu threads\n",
                        s.tids.size());
            return;
        }
    } else {
        HintSession* fresh = s.api.create_session(s.manager, s.tids.data(), s.tids.size(),
                                                  s.target_ns);
        if (!fresh) {
            std::printf("ADPF: the session could not be made again with %zu threads\n",
                        s.tids.size());
            return;
        }
        s.api.close_session(s.session);
        s.session = fresh;
        if (s.api.set_prefer_power_efficiency) {
            s.api.set_prefer_power_efficiency(s.session, false);
        }
    }
    s.session_threads = s.tids.size();
    std::printf("ADPF: session with %zu threads, target %.1f ms\n", s.session_threads,
                double(s.target_ns) / 1e6);
}

void Add(std::int32_t tid) {
    State& s = S();
    if (std::find(s.tids.begin(), s.tids.end(), tid) == s.tids.end() && s.tids.size() < 8) {
        s.tids.push_back(tid);
        s.dirty.store(true, std::memory_order_release);
    }
}

// ---- Thermal ----
const char* ThermalName(int status) {
    static const char* const names[] = {"none", "light", "moderate", "severe",
                                        "critical", "emergency", "shutdown"};
    return status >= 0 && status < 7 ? names[status] : "unknown";
}

void ThermalLoop() {
    State& s = S();
    pthread_setname_np(pthread_self(), "bb:Thermal");
    ThermalManager* manager = s.api.thermal_acquire();
    if (!manager) {
        std::printf("Thermal: off (AThermal_acquireManager returned null)\n");
        return;
    }
    int last = -2;
    while (true) {
        const int status = s.api.thermal_status(manager);
        if (status != last) {
            last = status;
            // The headroom (API 31): 1.0 is where the device starts to throttle; NaN: unknown.
            const float headroom = s.api.thermal_headroom ? s.api.thermal_headroom(manager, 0)
                                                          : NAN;
            if (std::isfinite(headroom)) {
                std::printf("Thermal: status %s (%d), headroom %.2f\n", ThermalName(status), status,
                            double(headroom));
            } else {
                std::printf("Thermal: status %s (%d), headroom unknown\n", ThermalName(status),
                            status);
            }
            std::fflush(stdout);
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void StartThermal() {
    State& s = S();
    static std::once_flag started;
    std::call_once(started, [&] {
        if (!s.enabled || EnvOff("BB_THERMAL_LOG")) {
            return;
        }
        if (!s.api.thermal_acquire || !s.api.thermal_status) {
            std::printf("Thermal: off (no AThermal in libandroid.so)\n");
            return;
        }
        std::thread(ThermalLoop).detach();
    });
}
} // namespace

void RegisterMainThread() {
    Init();
    State& s = S();
    if (!s.enabled) {
        return;
    }
    {
        std::scoped_lock lk{s.lock};
        if (!s.have_main) {
            s.have_main = true;
            const int tid = int(gettid());
            Add(tid);
            OpenProbe(s.main_probe, tid);
        }
    }
    StartThermal();
}

void NoteThreadName(const char* name) {
    if (!name) {
        return;
    }
    // Cheap rejection before anything is loaded: only these names matter.
    const bool match = !std::strncmp(name, "shadPS4:GpuComm", 15) || !std::strcmp(name, "bb:DrawRec") ||
                       !std::strncmp(name, "bb:VkRecorder", 13) || !std::strncmp(name, "GXRender", 8);
    if (!match) {
        return;
    }
    Init();
    State& s = S();
    if (!s.enabled) {
        return;
    }
    const int tid = int(gettid());
    std::scoped_lock lk{s.lock};
    if (!std::strncmp(name, "shadPS4:GpuComm", 15)) {
        if (!s.have_gpu) {
            s.have_gpu = true;
            Add(tid);
            OpenProbe(s.gpu_probe, tid);
        }
    } else if (!std::strcmp(name, "bb:DrawRec")) {
        if (!s.have_draw) {
            s.have_draw = true;
            Add(tid);
        }
    } else if (!std::strncmp(name, "bb:VkRecorder", 13)) {
        if (s.recorders < 2) { // the first two workers
            ++s.recorders;
            Add(tid);
        }
    } else if (!s.have_render) {
        s.have_render = true;
        Add(tid);
    }
}

void FrameSubmitted() {
    State& s = S();
    if (!s.dlopen_done.load(std::memory_order_acquire) || !s.enabled || s.failed) {
        return;
    }
    std::scoped_lock lk{s.lock};
    const u64 now = NowNs();
    if (!s.session || s.dirty.exchange(false, std::memory_order_acquire)) {
        EnsureSession();
        if (!s.session) {
            return;
        }
    }
    // The thread times at this frame, whether or not it is reported (the next frame's baseline).
    u64 work = 0;
    bool measured = false;
    for (Probe* p : {&s.main_probe, &s.gpu_probe}) {
        u64 total;
        if (!Sample(*p, total)) {
            continue;
        }
        if (p->valid && total >= p->last) {
            work = std::max(work, total - p->last);
            measured = true;
        }
        p->last = total;
        p->valid = true;
    }
    const u64 interval = s.last_frame_ns ? now - s.last_frame_ns : 0;
    s.last_frame_ns = now;
    // The target follows the frame limit (a change of it is passed on at once).
    const i64 target = TargetNs();
    if (target != s.target_ns) {
        s.target_ns = target;
        s.api.update_target(s.session, target);
    }
    // Not a frame of play: the first, a pause or a load (over a second), a doubled flip.
    if (!measured || interval < 500000ull || interval > 1000000000ull || work == 0) {
        return;
    }
    work = std::min(work, interval);
    s.api.report_actual(s.session, i64(work));
}
} // namespace BbAdpf

extern "C" void bbgpu_adpf_register_main(void) {
    BbAdpf::RegisterMainThread();
}

extern "C" void bbgpu_adpf_thread_named(const char* name) {
    BbAdpf::NoteThreadName(name);
}
