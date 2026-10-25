# Android baseline build

The Android workflow builds revvv991's 0.2.4 baseline with its pinned FEX,
Mesa and renderer dependencies. The fork uses `com.daedalus.uncharted` to
coexist with the upstream app. No rendering or game compatibility changes
are included in this baseline.

`development-key.base64` is an intentionally public development signing key,
retained so CI test builds can update one another. It is not a production
release key. Do not use it to sign a public production release.

The workflow verifies the APK signature, application ID and ARM64 ABI and
uploads the APK, its SHA-256 and exact source commit. This does not validate
game boot, drivers, audio, graphics or performance on a physical device.
