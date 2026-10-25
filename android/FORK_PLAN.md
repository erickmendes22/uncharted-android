# Daedalus Android integration checkpoints

## Baseline

Selected Android source: revvv991/uncharted-android v0.2.4,
`6d1e1ff4780337ec07c12142d175d7ece214a85c`.
The first CI build preserves its runtime and renderer. Its separate application
ID is `com.daedalus.uncharted`. CI compilation and APK verification do not prove
game compatibility on a physical phone.

Keep the mobile stage A/B command pipeline, skinning batches, submission wakeup
changes, 8 Elite affinity handling, video decoder optimizations, FSR UMA allocation
fallback, frame-generation resource cleanup and U2 tiny autofocus readback.

## Selective RedVorteX integration

Reference: RedVorteX-TV/Uncharted-arm, branch `android`.

* LSFG is integrated through `vk_frame_generation.cpp`, its header, settings,
  Vulkan device features and the `renderer_vulkan/lsfg` sources. Copying the
  presenter alone is insufficient. Keep the base's Android swapchain and frame
  retirement behavior; the reference also has a separate Android AHB path.
* Enable and check Vulkan memory model support before selecting LSFG. Keep
  shader-float16 support conditional. Missing DLL or unsupported features must
  leave ordinary presentation working.
* The DLL and derived shaders must never be bundled. The LSFG sources require
  GPL-3.0-or-later licensing and attribution for the combined binary.
* Driver import and download already exist in the chosen baseline. Add GPU
  family detection and selection to those paths rather than duplicate them.
  RedVorteX's detection helper expects `bionic/bin/kgsl-chip-id`; it cannot be
  copied into this launcher without adapting that dependency.
* Do not replace the texture cache with the reference version: it lacks the
  baseline's `BB_TINY_READBACK` path for U2 autofocus.

## PC fixes to examine separately

Reference: Supermedo/uncharted_pc v0.3.9,
`754ed2ab4f1fdd21250c8785d4d4f49517d31292`.

* U3 cloth shader barriers: inspect lane/subgroup assumptions and memory flags
  on Adreno before enabling; the PC report concerns NVIDIA's 32-lane execution.
* Invalid FMask stores/atomics and descriptor fallback: port the small compiler
  changes rather than overwrite Android's SRT and GPU command handling.
* U3 player focus NaN recovery (1.00 and 1.02): PC uses instruction-pattern
  matching, while this Android baseline's `RuntimeGuestCallback` table still
  uses fixed offsets plus expected bytes. Adapt the lookup and validation before
  adding those recipes. Never apply the byte sequences at guessed offsets.
* PC U2 focus/leg-IK recovery also extends coverage to build 1.02. Preserve the
  existing Android build 1.00 safeguards when extending coverage.
* Windows reserve/commit and error-1450 recovery are not Android changes.

## Device validation

Record GPU, driver/version, Android version and page size. Test all three games
without frame generation first, including gameplay, video, sound, controller
reconnection and U2 autofocus. Compare LSFG only after that baseline is known.
Do not describe bundled Adreno 8xx or Mali drivers as validated from their mere
presence in another port.
