# Additional performance and storage audit — Daedalus 0.2.5

Base: `e46d2442a2596fdd33303e01acd036e7982181cd`, the asynchronous U2 fix.
The earlier synchronous correction was reported to stop the crash. Net FPS and
stability of further changes require comparison at the same checkpoint on a phone.

## Pinned comparisons

| Source | Revision | Relevant result |
|---|---|---|
| [Supermedo PC 0.3.9](https://github.com/Supermedo/uncharted_pc/commit/754ed2ab4f1fdd21250c8785d4d4f49517d31292) | 754ed2ab | Protected-page asset reads and ordered mesh result delivery already adapted. Its full global readback settings must not overwrite our policy. |
| [RedVorteX Android/ARM 0.1.6](https://github.com/RedVorteX-TV/Uncharted-arm/commit/c6b0f9007b8ee2b69e97093f38892f47b5d7d5dc) | c6b0f900 | Batched skinning, render CPU selection and constant deduplication are already present; do not count them twice. |
| [revvv991 Android 0.2.8](https://github.com/revvv991/uncharted-android/commit/b708477150447ffb1ddfe5d1ceeb5c9e95927adf) | b7084771 | New automatic stall reports/tracing are diagnostics, not measured performance improvements. 0.2.7 also changes accurate GPU memory defaults; those defaults are deliberately not imported. |

## Candidate decisions

| Candidate | Decision | Reason |
|---|---|---|
| Batched skinning, stream dedupe, render affinity, fewer guest wakeups | Already present | Existing release base has these paths and launch settings. |
| Reusable staging for asynchronous GPU results | Implemented as a focused adaptation | Upstream small-result readbacks use reusable storage. Our owned U2 snapshots previously allocated/freed a Vulkan buffer on each copy. Reuse only after GPU timeline completion **and** the CPU copy; completion gates remain unchanged. |
| Mapped UMA readback | Keep existing optional implementation | It already checks completed writes and emits host barriers. Do not turn on precise readback globally or remove the fallback. |
| Late delivery of <=64-byte GPU counters beside CPU writes | Not enabled | Upstream uses this for accurate GPU memory. It is a different coherency path and does not establish a gain under our current merge policy. |
| Disabling the two-stage draw pipeline / narrowing constant reads | Not changed | Different devices and scenes have different tradeoffs. Disabling pipeline synchronization or reducing copied ranges speculatively can reintroduce stale data. |
| Tiled light descriptor and NaN clamp corrections | Separate graphics work | Correctness fixes, not demonstrated FPS gains; do not mix shader/cache changes into this performance/storage comparison. |
| Continuous HLE/fiber traces and automatic stall monitor | Not imported | No direct FPS gain; avoid adding instrumentation to the normal run. |

## Snapshot reuse invariants

At most 16 results are pending. An idle snapshot is selected by best fit;
on a size miss an idle buffer is retired before allocating a replacement.
Allocation rounds to a power of two, 4 KiB minimum, 2 MiB maximum. Active plus
idle buffers remain bounded at 16 (32 MiB logical maximum; driver allocation
granularity may add overhead). A snapshot returns to the reuse pool only after
its timeline tick completes and its data is copied to guest memory. In-flight
buffers cannot alias. No readback mode, FPS limit, shader, asset patch, FSR,
audio or game logic changes are included.

The tests compile the real landing worker and verify data-before-label, signal
ordering, overlap and cancellation. The staging test verifies identity reuse,
best fit, no in-flight reuse, size replacement and repeated reuse.

## Folder and PKG installation

References:
- [GoW3 folder manager](https://github.com/erickmendes22/gow3_android_daedalus_fork/blob/b3a7c3d5e916cf8f4622192c0faa9383ebd77c00/android-launcher/app/src/main/java/com/gow3/android/data/GameFolderManager.kt)
- [GoW3 PKG installer](https://github.com/erickmendes22/gow3_android_daedalus_fork/blob/b3a7c3d5e916cf8f4622192c0faa9383ebd77c00/android-launcher/app/src/main/java/com/gow3/android/data/PkgInstallManager.kt)
- [inFAMOUS Bionic installer](https://github.com/erickmendes22/iss_android/blob/acf0feb38cd621259ad16fb700fa202d3725cd23/android/app/src/ro/cobrabm/fexdroid/BionicPkgInstaller.java)

Our installer was already adapted from inFAMOUS: direct extraction into an
app-created game folder, fresh numbered folders rather than overwriting a base,
and updates only after TITLE_ID matching. Those safeguards remain in place.

Additional changes:

- All folder buttons request file access when needed and resume the requested
  action after granting it, with a fallback to the general storage settings.
- Folder and PKG selection share local-volume resolution, including case-insensitive
  `primary`, canonical containment and bounded discovery of the collection/CUSA
  directory inside the selected parent. Persist URI grants when supplied.
- Downloads/OEM providers may omit `_data` or provide an unusable path. Keep a
  regular, seekable provider descriptor alive for both native helper processes.
  ProcessBuilder opens the descriptor path in the parent and redirects it to
  child stdin; the extractor opens `/proc/self/fd/0`, avoiding access to another
  process's proc files. Pipes/non-seekable sources are rejected. No PKG byte copy
  into app storage and no extra extraction staging folder are introduced.
- The native extractor still needs a directly writable destination. A cloud tree
  or an SD-card tree granting only SAF writes is not silently claimed supported.
  Real destination write probes and extraction validation remain mandatory.

OS-level regression checks prove two helper processes can seek and mmap a
retained descriptor after its ordinary filename is gone. Java tests cover
primary-volume casing, roots, traversal and symlink escape rejection, and bounded
nested game-folder discovery. Android CI compiles the real Java/NDK code; OEM
provider and SELinux behavior still require the affected phone's validation.
