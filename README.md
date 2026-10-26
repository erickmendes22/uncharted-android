# Uncharted Daedalus

Android port of **Uncharted: The Nathan Drake Collection**, featuring a **Bionic runtime**,
**FEX CPU translation** and **native Vulkan rendering**.

**Version 0.2.5** · [Downloads](https://github.com/erickmendes22/uncharted-android/releases)
· [Release source](https://github.com/erickmendes22/uncharted-android/tree/android-ui-runtime-pending-xml)

Includes **Drake's Fortune**, **Among Thieves** and **Drake's Deception**.

## Features

- Uncharted-themed launcher and in-game settings.
- Redesigned touch controls with adjustable size, opacity and camera sensitivity,
  plus physical controller support.
- PKG installation and updates, game-folder selection, save backup/restore and
  log export to a folder of your choice.
- Optional Fullscreen, configurable frame limits and VSync.
- Minimal, Compact and Full performance HUD modes, with positioning and scale
  settings up to **300%**.
- **LSFG 2x / 3x / 4x** using your own `Lossless.dll`. The HUD reports measured
  original and output FPS, for example **FPS 30→60 LSFG 2x**. Text automatically
  fits the available width to keep the complete line visible.
- FSR 3.1 temporal reconstruction and TAA, with sharpening controls.
- Resolution and graphics patches for supported game versions **1.00 and 1.02**,
  checked against your executables before applying.
- Bundled Turnip drivers, custom driver ZIP import and system-driver selection.
- Adaptive texture-memory management, texture-cache fixes, improved frame pacing
  and Android performance hints.
- Android game-category and Game Mode integration.
- Automatic game-file preparation when playing and a launcher that adapts to
  screen rotation.

## FSR 3.1 presets

All temporal presets reconstruct to **1280 × 720**. The input resolution changes
with the selected preset.

| Preset | Internal resolution | FSR output |
| --- | --- | --- |
| Native AA | 1280 × 720 | 1280 × 720 |
| Quality | 960 × 540 | 1280 × 720 |
| Balanced | 832 × 468 | 1280 × 720 |
| Performance | 640 × 360 | 1280 × 720 |

Resolution changes apply on the next game start. With upscaling disabled, the
manual resolution setting applies. LSFG is the available frame-generation option.

## Languages

English, Portuguese, Spanish, French, German, Italian, Russian, Japanese, Korean,
Simplified Chinese, Turkish and Polish.

**Automatic** follows the system language and falls back to English when it is
unsupported. The selected language also applies to the game on its next start;
available text and audio depend on the languages included in your game files.

## Requirements

- **Android 13 or newer** and an **ARM64** device.
- A compatible Vulkan GPU and driver; Turnip requires a supported Adreno GPU.
- A kernel using **4 KB memory pages**, as required by FEX.
- Your own decrypted game files or supported game PKG.

No game files, firmware or `Lossless.dll` are included. Performance depends on
the device, driver, game and settings. Game Mode and game-dashboard availability
also depend on the Android device.

## Getting started

1. Install the APK and grant storage access when prompted on the first opening.
2. Choose your decrypted collection folder or use **Install game PKG**.
   Use **Install update PKG** for a supported update.
3. Select one of the three games, then choose your driver, graphics and control settings.
4. Tap **Continue Adventure**. Required runtime files are prepared automatically.
5. Open the in-game menu using its on-screen button, a swipe from the right edge
   or **L3 + R3** on a controller. It includes settings, log export and **Exit game**.

To use LSFG, import your own `Lossless.dll` before selecting a multiplier.
Keep backups of your saves before uninstalling the app: Android removes app data
during uninstallation.

## Credits

- Android fork and modifications: [erickmendes22](https://github.com/erickmendes22) /
  [Daedalus Emulator](https://www.youtube.com/@daedalusemulator).
- Android base: [revvv991/uncharted-android](https://github.com/revvv991/uncharted-android).
- Native port: [Supermedo/uncharted_pc](https://github.com/Supermedo/uncharted_pc).
- Mobile memory and performance integrations:
  [ducvd89/Bloodborne-Android](https://github.com/ducvd89/Bloodborne-Android).
- Selected Android integrations:
  [RedVorteX-TV/Uncharted-arm](https://github.com/RedVorteX-TV/Uncharted-arm).
- Upstream cache and frame-pacing fixes:
  [spacepepperoni/uncharted_Linux](https://github.com/spacepepperoni/uncharted_Linux).
- GPU/runtime foundations: [shadPS4](https://github.com/shadps4-emu/shadPS4),
  [FEX-Emu](https://github.com/FEX-Emu/FEX) and
  [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc).
- [Mesa / Turnip](https://gitlab.freedesktop.org/mesa/mesa),
  [libadrenotools](https://github.com/bylaws/libadrenotools),
  [SDL3](https://github.com/libsdl-org/SDL) and [FFmpeg](https://ffmpeg.org).
- Resolution patch foundations:
  [Bachata S4](https://github.com/zenithblue-oss/shadps4-arm64).

Thanks to all upstream contributors. See the in-app licenses,
[LICENSE](LICENSE), [LICENSE.GPL-3.0](LICENSE.GPL-3.0) and
[Android component licenses](android/LICENSES.md).
Not affiliated with Sony Interactive Entertainment or Naughty Dog.

## Development

[Android build instructions](https://github.com/erickmendes22/uncharted-android/blob/android-ui-runtime-pending-xml/android/README.md)
· [0.2.5 integration review](https://github.com/erickmendes22/uncharted-android/blob/android-ui-runtime-pending-xml/android/RELEASE_0.2.5_AUDIT.md)
· [Original PC documentation](README.uncharted_pc.md)
