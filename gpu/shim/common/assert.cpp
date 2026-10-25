// bbport: GPU-side assertion failures stop the port with exit code 23.
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "common/assert.h"
#include "common/logging/log.h"

#ifndef _WIN32
namespace Vulkan {
void PrintHostBacktrace(const char* prefix); // vk_semaphore.cpp
}
#endif

void assert_fail_impl() {
#ifndef _WIN32
    // g2-arm64: where it failed, as module offsets (a bug report's log has no other trace).
    Vulkan::PrintHostBacktrace("Assertion at");
#endif
    std::fflush(stdout);
    std::fputs("STOP: GPU library assertion failed (see GPU log above)\n", stderr);
    std::_Exit(23);
}

[[noreturn]] void unreachable_impl() {
    assert_fail_impl();
    throw std::runtime_error("Unreachable code");
}

void assert_fail_debug_msg(const char* msg) {
    LOG_CRITICAL(Debug, "Assertion Failed!\n{}", msg);
    assert_fail_impl();
}
