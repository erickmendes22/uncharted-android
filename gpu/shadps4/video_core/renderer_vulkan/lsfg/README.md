# LSFG frame generation

Lossless Scaling's frame generation (LSFG) for the frames the presenter shows. It runs inside the
port's own present path, with no Vulkan layer.

## Lossless.dll is not included

The shaders come from the player's own copy of `Lossless.dll`, from Lossless Scaling, which they
have bought. Nothing from Lossless Scaling is bundled or distributed: no DLL, no shaders, no
shader cache. The player chooses the file:

- `lsfg_dll` in the settings, or `BB_LSFG_DLL`;
- frame generation set to `lsfg` (`BB_FRAME_GEN=lsfg`, or the L3+R3 menu).

The shaders are read from the DLL's RCDATA resources and translated in memory each time the game
starts (LosslessDll.cpp, LsfgTranslate.cpp).

## Where it comes from

| Files | Origin |
| --- | --- |
| FrameGen*, Lsfg{Alpha,Beta,Gamma,Delta,Generate,Mipmaps,Chain,Common,Shaders,Translate,Util}*, LosslessDll* | Eden's frame generation, eden-emu PR #4263 (Copyright 2026 Eden Emulator Project), on the passes of lsfg-vk (Copyright 2025 lsfg-vk), as ARMSX2 ported them to its own renderer (ARMSX2/ARMSX2 commit 90126bdfea7a3ffb1fd288b2f7f0ae0b033af2d6) |
| LsfgVkCompat* | ARMSX2's stand-in for Eden's Vulkan wrappers |
| LsfgHost.h, LsfgRunner.*, FrameGenTypes.h | this port |

### Changes for this port

- ARMSX2's `GSDeviceVK` became `LsfgHost`. It provides the device, the features that were enabled
  on it, and an idle wait that holds the renderer's submit lock.
- `GSConfig` became `LsfgConfig()` (FrameGenTypes.h), filled on each frame from the port's settings.
- The on-disk shader cache was removed, so nothing derived from the DLL is ever written.
- `LsfgRunner` is the only interface the renderer uses. Only Vulkan handle types cross it, which
  keeps vulkan.hpp out of these files.

## License

These files are GPL-3.0-or-later, as their sources are. As a result, the port's binaries, which
link them, are distributed under GPL-3.0-or-later as a whole. See THIRD_PARTY_LICENSES.md at the
root of the repository.
