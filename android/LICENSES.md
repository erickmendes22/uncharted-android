# Licenses

The app (the port and its Android parts in this repository) is distributed under the
**GNU General Public License, version 2 or later** (`LICENSE` at the repository root), as
uncharted_pc and shadPS4 are. Its source is this repository plus the patches for FEX and Mesa in
`scripts/port-arm64/patches/`.

The APK contains these components:

| Component | Where | License |
|---|---|---|
| uncharted_pc (the port: loader, PS4 library replacements) | `src/`, `gpu/shim/` | GPL-2.0-or-later |
| shadPS4 (video core, shader recompiler; vendored) | `gpu/shadps4/` (`gpu/VENDOR.txt`) | GPL-2.0-or-later |
| FEXCore and its bundled libraries | `~/src/fex` (branch g2-android, patches here) | MIT; fmt (MIT), xxHash (BSD-2-Clause), jemalloc (BSD-2-Clause), Berkeley SoftFloat 3e (BSD-3-Clause), Cephes (BSD), rpmalloc (public domain or MIT), tiny-json (MIT), unordered_dense (MIT), range-v3 (BSL-1.0), cpp-optparse (MIT) |
| Mesa Turnip (`libvulkan_freedreno.so`) with the KGSL patches | Mesa (patches here) | MIT |
| libadrenotools (loads Turnip) | bylaws/libadrenotools | BSD-2-Clause |
| SDL3 (window, input, audio, the Java activity) | libsdl-org/SDL | Zlib |
| FFmpeg n9.0.2: libavcodec, libavformat, libavutil, libswresample, libswscale (movies; built without GPL or non-free parts) | FFmpeg | LGPL-2.1-or-later |
| LLVM libc++ (`libc++_shared.so`, from the NDK) | LLVM | Apache-2.0 WITH LLVM-exception |
| LibAtrac9 (sound) | `third_party/LibAtrac9` | MIT |
| sirit (SPIR-V emitter) | `gpu/third_party/sirit` | BSD-3-Clause |
| Vulkan Memory Allocator, Vulkan headers | GPUOpen, Khronos | MIT; Apache-2.0 or MIT |
| magic_enum, miniz, half, Dear ImGui, robin-map, FSR for Vulkan | `gpu/third_party` | MIT |
| Boost (headers) | boost.org | BSL-1.0 |
| Zydis | zyantific/zydis | MIT |
| xbyak | `gpu/third_party/xbyak` | BSD-3-Clause |
| DejaVu fonts (the port's overlay) | `gpu/third_party/fonts` | Bitstream Vera license, public domain changes |

Not part of the app and not distributed: the game and everything made from it (the prepared
`boot-linked.bin`, caches, the byte patches). The app carries resolution recipes
(`scripts/g2/recipes/`): addresses, SHA-256 hashes of the original bytes and only the new bytes;
the launcher makes the patches from the user's own files. Our patch XMLs
(`scripts/g2/u1_extra_patches.xml`, `scripts/g2/u23_extra_patches.xml`) hold hashes of the
original bytes too, not the bytes.
