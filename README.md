# Uncharted Daedalus

Android port of **Uncharted: The Nathan Drake Collection**, featuring a Bionic runtime,
FEX CPU translation and native Vulkan rendering.

**Version: 0.2.9** · [Downloads](https://github.com/erickmendes22/uncharted-android/releases)

- Uncharted-themed launcher, touch controls and in-game settings.
- English, Portuguese, Spanish, French, German, Italian, Russian, Japanese, Korean,
  Simplified Chinese, Turkish and Polish. Automatic language follows the system,
  falling back to English. The selected language is passed to the game at startup;
  available game text and audio depend on your copy.
- PKG installation and updates, game-folder selection, ZIP save import/export via Android file picker (with pre-import local backup), legacy backup restore and log export.
- Fullscreen, configurable frame limits and a performance HUD with up to 300% scale.
- FSR 3.1 temporal reconstruction to **1280×720**: Native AA (720p), Quality (540p),
  Balanced (468p) and Performance (360p).
- Manual home-screen resolution only with FSR off; resolution/effect patches for supported **1.00 and 1.02** executables, checked
  against the user's own files before applying.
- Optional LSFG frame generation using your own `Lossless.dll`; FSR frame generation
  is disabled in this Android port.
- Automatic Turnip Gen8 V38 Patched for Adreno 8xx, bundled Turnip, driver ZIP import and manual system-driver selection.
- Adaptive texture-memory budgeting and Android performance hints, with Android
  game-category and Game Mode integration.

## Requirements and setup

Android 13 or newer, ARM64, a compatible Vulkan GPU and your own decrypted game
files. No game files, firmware or `Lossless.dll` are included. Select the game
folder or install its PKG, choose one of the three games and start it. Keep backups
of your saves before uninstalling the app.

FPS and image quality depend on the phone, driver, game and settings. Game Mode
availability and game-dashboard behavior depend on the Android device.

The package remains `com.daedalus.uncharted`. This version preserves the signing identity and uses version code 100209, allowing
installation over earlier signed builds without deleting app data.

## Credits

- Android fork and modifications: [erickmendes22](https://github.com/erickmendes22) /
  [Daedalus Emulator](https://www.youtube.com/@daedalusemulator).
- Android base: [revvv991/uncharted-android](https://github.com/revvv991/uncharted-android).
- Native port: [Supermedo/uncharted_pc](https://github.com/Supermedo/uncharted_pc).
- Mobile memory and performance integration:
  [ducvd89/Bloodborne-Android](https://github.com/ducvd89/Bloodborne-Android).
- Selected Android integrations:
  [RedVorteX-TV/Uncharted-arm](https://github.com/RedVorteX-TV/Uncharted-arm).
- GPU/runtime foundations: [shadPS4](https://github.com/shadps4-emu/shadPS4),
  [FEX](https://github.com/FEX-Emu/FEX),
  [deadinside28/bloodborne_pc](https://github.com/deadinside28/bloodborne_pc).
- Upstream cache/pacing fixes:
  [spacepepperoni/uncharted_Linux](https://github.com/spacepepperoni/uncharted_Linux).

See the in-app licenses, [LICENSE](LICENSE) and [LICENSE.GPL-3.0](LICENSE.GPL-3.0).
Not affiliated with Sony Interactive Entertainment or Naughty Dog.

Build instructions: [android/README.md](android/README.md).
Integration review: [android/RELEASE_0.2.5_AUDIT.md](android/RELEASE_0.2.5_AUDIT.md).
