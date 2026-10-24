// g2-arm64: build without the network/PSN libraries (BB_NET=OFF, Android). The games import a
// few libSceNpTus functions (cloud variables): offline versions with the semantics of shadPS4's
// np_tus.cpp, which the PC build uses without a server (requests finish at once with result 0).
#include <atomic>
#include "common/types.h"
#include "core/libraries/libs.h"

namespace {
constexpr s32 kInsufficientArgument = static_cast<s32>(0x8055070C); // ORBIS_NP_COMMUNITY_ERROR_*
std::atomic<s32> next_context{1}, next_request{1};

s32 PS4_SYSV_ABI TusCreateNpTitleCtx(u32 service_label, const void* np_id) {
    if (!np_id) {
        return kInsufficientArgument;
    }
    (void)service_label;
    return next_context++;
}
s32 PS4_SYSV_ABI TusDeleteNpTitleCtx(s32) { return 0; }
s32 PS4_SYSV_ABI TusCreateRequest(s32) { return next_request++; }
s32 PS4_SYSV_ABI TusDeleteRequest(s32) { return 0; }
s32 PS4_SYSV_ABI TusAsync() { return 0; } // Get/Set data, friends' status: done, nothing stored
s32 PS4_SYSV_ABI TusPollAsync(s32, s32* result) {
    if (result) {
        *result = 0;
    }
    return 0; // finished
}
} // namespace

extern "C" void bbnet_register(void) {
    Core::Loader::SymbolsResolver resolver;
    Core::Loader::SymbolsResolver* sym = &resolver;
    LIB_FUNCTION("BIkMmUfNKWM", "libSceNpTus", 1, "libSceNpTus", TusCreateNpTitleCtx);
    LIB_FUNCTION("H3uq7x0sZOI", "libSceNpTus", 1, "libSceNpTus", TusDeleteNpTitleCtx);
    LIB_FUNCTION("3bh2aBvvmvM", "libSceNpTus", 1, "libSceNpTus", TusCreateRequest);
    LIB_FUNCTION("CcIH40dYS88", "libSceNpTus", 1, "libSceNpTus", TusDeleteRequest);
    LIB_FUNCTION("uHtKS5V1T5k", "libSceNpTus", 1, "libSceNpTus", TusAsync);  // GetDataAsync
    LIB_FUNCTION("G68xdfQuiyU", "libSceNpTus", 1, "libSceNpTus", TusAsync);  // SetDataAsync
    LIB_FUNCTION("LUwvy0MOSqw", "libSceNpTus", 1, "libSceNpTus", TusAsync);  // GetFriendsDataStatusAsync
    LIB_FUNCTION("t7b6dmpQNiI", "libSceNpTus", 1, "libSceNpTus", TusPollAsync);
}
extern "C" void bbnet_configure(const char*, const char*, const char*, const char*) {}
