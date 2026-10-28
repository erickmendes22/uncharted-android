# Six renderer improvements: equivalence review and integration

Base: c358044e518037d33f67f0ffc15cb42fb1ecb68a, the published release-base
line with the U2 crash/loading protections, owned asynchronous result snapshots,
staging reuse, shared PKG/folder mechanism and corrected first-run setup.
App version, package and signing identity stay 0.2.5 / 100205 /
com.daedalus.uncharted / the existing signing key.

## Sources examined

- [Supermedo/uncharted_pc](https://github.com/Supermedo/uncharted_pc/tree/754ed2ab4f1fdd21250c8785d4d4f49517d31292), PC 0.3.9.
- [revvv991/uncharted-android](https://github.com/revvv991/uncharted-android/tree/b708477150447ffb1ddfe5d1ceeb5c9e95927adf), Android 0.2.8.
- [RedVorteX-TV/Uncharted-arm](https://github.com/RedVorteX-TV/Uncharted-arm/tree/c6b0f9007b8ee2b69e97093f38892f47b5d7d5dc), Android 0.1.6.

The Android upstream's adaptation of the PC compiler/cache work was used rather
than replacing our compiler with the PC files: the latter loses our CS split
and wave64 subgroup selection on Turnip. Diff/merge-base statistics alone were
not treated as proof that a mechanism was missing.

## Equivalence gate

| Item | Existing mechanism | Missing portion actually integrated | Default |
| --- | --- | --- | --- |
| 1 | Serialized shader SPIR-V, metadata and pipeline keys; incompatible profile rebuild | Native Vulkan driver blob persisted atomically, prevalidated vendor/device/cache UUID; separate GPU directories | On; configurable |
| 2 | Draw scanner and speculative preparation workers; shared program lookup | Background cached-pipeline preload and optional compilation of graphics/compute shaders and pipelines, per-program/per-pipeline claims | Both options off |
| 3 | 32-entry T# table plus complete light records | All 128 light-cookie descriptors, 8244-dword window, 192 bound-image capacity | On; configurable |
| 4 | EXEC divergence construction | Split eligible scalar ALU instructions out of EXEC-masked scopes | On; configurable |
| 5 | Final colour-export NaN guard; optional vertex clamp | Fragment saturate NaN-to-zero through NMin/NMax, preserving both earlier protections | On; configurable |
| 6 | U2 1x1 focus readback and mesh-result owned snapshots | Bounded small compute-written result readbacks, U3 only, asynchronous and batched | Off |

No full replacement of an existing equivalent mechanism was necessary. In
particular item 6 uses the existing owned-snapshot queue/pool, timeline waits on
the landing worker and data-before-label gates; it does not import a second
readback worker, staging ring, side queue or global precise readback setting.

## Mobile adaptation and retained safeguards

- No BB_GPU_MERGE override or global BB_SIDE_DOWNLOAD override. Existing U2
  BB_SCALAR_COPY64_SYNC/DEFER and BB_TINY_READBACK policies are unchanged.
- Readback snapshots retain the common 16-pending and 2 MiB-per-snapshot bounds.
  New small readbacks are at most 64 bytes, 16 distinct ranges per event, batched
  into one graphics submission; no synchronous fallback when this optional
  queue is full. They are retried at a later event. U2's own fallback is retained.
- Compiler workers are capped at two when enabled, with per-worker pools and
  compute split selection; the GPU thread's split state is never read by workers.
  Worker compilation off retains the previous preparation worker count/priority.
- Background preload uses at most two threads (one for archive caches); off
  performs serial preload. The application can start while background pipelines
  are built, but shader metadata/permutation slots must be ready before selection.
- Per-pipeline claims prevent duplicate runtime/preload/worker compilation,
  including compute pipelines. Program objects and shader modules remain stable.
- Shared walker generation is serialized. A speculative resource-table fault
  abandons that speculative attempt without permanently zeroing either the native
  walker or the ARM interpreter's shared program. The real GPU thread's original
  fallback remains intact.
- Session-local object-motion vertex shaders are still rejected on cache load.
  No PC address-relocation/motion shader cache mechanism is imported.
- Vulkan driver blobs are limited to 128 MiB on load. Header size/version,
  vendor/device ID and UUID are validated before passing bytes to the driver;
  invalid blobs fall back to an empty cache. Capture is synchronized against all
  pipeline creation using that cache; disk writes use a temporary file and rename.
  Periodic saves and explicit quit flush are supported; forced process termination
  may lose changes since the last periodic save.
- Graphics comparison flags each use a separate, versioned shader cache namespace.
  New SPIR-V/meta versions prevent old cached shaders from hiding a correction.
  Old cache directories are not automatically deleted.
- Seven launcher switches expose native cache, background preload, worker
  compilation, full light table, scalar scopes, material clamp, and U3 readbacks.
  All switches apply at next game start/loadexec. U2/U1 cannot inherit U3 readbacks.
  New labels/help are in the shared 12-language catalog.

## Deliberately excluded

Already equivalent/better here: skinning batching, stream deduplication, CPU
affinity, bounded memory cleanup, image hash handling, fixed invalidated texture
references, non-null record chunks, four-thread/NEON cutscene decoding, blur
scaling, tiny U2 focus readbacks and protected streaming reads. Current FSR,
controls/HUD, PKG installation and first-run flow are preserved.

Not part of these six items: skip-failed-pipeline/draw behavior, null vertex
resource skipping, global stable SlotVector, A735-only Mesa page skipping,
Windows/NVIDIA-only fixes, extra diagnostics, trophy/updater features, casefold
paths and ELF preparer changes. No precise readback is enabled to obtain any
of these features.

## Validation and limits

Production-method regression tests exercise the landing worker/gates and ARM
SRT speculative recovery. Header tests reject incompatible/truncated Vulkan
blobs and exercise 24 unique graphics cache identities. Java policy tests cover
all 384 game/setting combinations, stale settings removal and U3-only readbacks.
The release-base guard still checks the original runtime audio, FEX trap,
readback queue policy, HUD/log and GPU build baseline.

Android compilation/packaging and on-device graphics/performance are separate
checks. Do not describe device performance or visual correctness as measured
until the user tests the APK. Each graphics mode's first run rebuilds shaders;
compare warmed runs in the same scene, with the same resolution/FSR/driver.
