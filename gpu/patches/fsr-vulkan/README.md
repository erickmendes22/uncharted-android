# Local changes to gpu/third_party/fsr-vulkan

The submodule points at upstream FireBurn/FSR-Vulkan (`c64f093`). `build.sh` applies the
patches here to its working tree when they are not applied yet:

- `0001-...`: `BB_FSR4_PROFILE` (GPU time per FSR 4 pass) and `BB_FSR4_STATS` (driver
  statistics of each pass) in the FSR 4 v07 provider.
- `0002-...`: `#include <mutex>` in the FFX Vulkan backend; libc++ (the Windows MSYS2 CLANG64
  build) does not include it transitively.
- `0003-...`: FSR 3.1 frame generation tags the bridge's per-frame constant buffers and
  descriptor pools with the frame (`BridgeBeginFrame`) and frees them when the frame is retired
  (`BridgeRetireFrame`), as the upscaler does. Without it every generated frame leaked about 14
  of them (a KGSL buffer object each on Android): the process reached the kernel's mapping
  limit after a few minutes and crashed in the overlay (g2-arm64, 2026-10-25). The Android build
  (`android/scripts/port-arm64/build_gpu_android.sh`) applies these patches too.
