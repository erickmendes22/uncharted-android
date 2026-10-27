# Android v0.1.6 comparison — Uncharted Daedalus 0.2.5

Reference: [RedVorteX-TV/Uncharted-arm v0.1.6](https://github.com/RedVorteX-TV/Uncharted-arm/releases/tag/v0.1.6).
The supplied Android log identifies **android-c303023**, so Android-specific code was
compared with commit `c303023c2af3898d467c9a7351d110d5c05bd4b6`, rather than assuming
the Linux release tag (`5733658`) was the APK's exact source.

| Mechanism | Daedalus before this integration | Change |
| --- | --- | --- |
| Mapped UMA readback, U2/U3 | Side-queue copies or shadow merging | Added direct reads of host-visible arena memory after completed GPU writes, noncoherent invalidation and safe full-readback fallback. U2/U3 use accurate memory mode. |
| Small GPU batches, U2/U3 | Guest submission count limit | Added draw/dispatch count limit: 100 with mapped readback; independent disable switch. This is a different limit. |
| Idle job workers | PAUSE/WFE and job-lock timeout | Added validated worker-loop hooks: 256 scans, then 20 us naps or the game's semaphore wait. Independent disable switch. |
| Batched skinning | Already present; `BB_SKIN_BATCH=1` | Retained, not counted as new. |
| Constant/stream deduplication | Already enabled by default | Retained. |
| Performance-core affinity | Already present | Retained, including the existing 8 Elite policy. |
| Fewer submission wakeups | Already present | Retained. |
| EOP tick wait patch | Same source implementation | No new patch: executable versions may match different patterns. |
| Resolution recipes | Existing validated 1.00/1.02 recipes and 720p FSR output | Preserved. Upstream's native 720p recipe is a different render configuration; no FPS equivalence claimed. |

The three compatibility switches are in Settings, enabled by default, and apply
on the next game start. U1 retains its existing GPU readback policy. Batch
submission defaults to zero when mapped readback is unavailable or disabled,
as upstream does. Turning mapped reads off on U2/U3 uses the accurate graphics
queue fallback, rather than re-enabling unsafe shadow merging.

## Adaptation to this fork

Our pinned FEX generic syscall path originally overwrote RCX/R11. The embedded
OS_GENERIC ABI now preserves these registers so replacement PAUSE hooks preserve
the game's state. Linux syscall ABIs retain architectural clobbers. Dependency
fingerprinting forces rebuilding FEX when these patches change, including when
Actions restores an older dependency cache.

The mapped-memory HOST_READ barrier is recorded in both normal and deferred
submission paths, since Daedalus enables deferred submission. Mapping or copying
failure retains the existing full readback path. No active GPU image is freed,
and no pending or future producer tick is waited on by its recording thread.

Graphics recovery, startup configuration snapshots and saves remain available.
The new settings appear in exported startup diagnostics. High-frequency job-spin
statistics remain opt-in (`BB_JOB_SPIN_STATS`). App version remains 0.2.5.

## Evidence and validation

The supplied RedVorteX log shows mapped GPU reads and worker naps enabled on
Adreno 830, with native/temporal FSR off. It is not a controlled comparison with
Daedalus. The screenshot identifies all three compatibility switches; they were
investigated separately rather than treating the existing job-lock timeout as
equivalent to worker sleep.

Local checks passed:

- All 24 game/switch combinations and U1/U2/U3 environment isolation.
- Worker loop branch/opcode validation, disabled/malformed fallback and callback
  register preservation, including RCX/R11 sentinels.
- The production scanner found both PAUSE sites in each supplied executable:
  U1 loop 0x3677ac; U2 0x4eccdc; U3 0x49f2fc (segment-relative offsets).
- Current/future/completed producer tick policy and batching limits.
- Graphics backup/reset/settings preservation and existing guest patch safety.
- New FEX patch applies cleanly to the pinned source; 416 localization entries generated.

Full Android compilation and the existing APK verification suite are required
before distribution. These checks do not execute Vulkan or FEX on a phone.
Performance improvement and the S25 persistent graphical symptom need device
validation; neither is claimed resolved by this integration.
