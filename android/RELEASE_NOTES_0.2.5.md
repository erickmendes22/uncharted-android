# Uncharted Daedalus 0.2.5

Android release for Uncharted: The Nathan Drake Collection.

- Uncharted-themed interface, touch controls and controller support.
- 12 interface languages, with the selected language passed to the game at startup.
- FSR 3.1 temporal upscaling to 720p with Native AA, Quality, Balanced and Performance modes.
- Configurable performance HUD up to 300%, fullscreen and frame limits.
- PKG installation/updates, save backups, driver ZIP import and log export.
- LSFG frame generation using your own Lossless.dll.
- Improved texture-cache management, adaptive RAM budgeting and Android performance hints.
- Cutscene/frame-pacing fixes and Android game classification.

Requires Android 13+, ARM64 and a compatible Vulkan GPU. No game files or Lossless.dll included.
Performance varies by device and game; new performance changes await device comparison.

[Android fork](https://github.com/erickmendes22/uncharted-android) ·
[Daedalus Emulator](https://www.youtube.com/@daedalusemulator) ·
[Android base](https://github.com/revvv991/uncharted-android) ·
[PC base](https://github.com/Supermedo/uncharted_pc) ·
[Bloodborne Android](https://github.com/ducvd89/Bloodborne-Android)

### Startup and display fixes
- Automatic game-file preparation when playing; removed the manual Prepare card.
- Requests storage access on first opening, with manual retry if declined.
- Frame-generation HUD reports measured original and output FPS with the selected multiplier.
- Rebuilds launcher layout after rotation while preserving its active page and background tasks.
- Refreshes the swapchain when display rotation changes from an identity transform.

These fixes retain version 0.2.5. Device rotation and gameplay need confirmation on hardware.
