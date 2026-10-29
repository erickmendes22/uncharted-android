# Readback stability and U3 patch corrections

Version/package/signing remain Uncharted Daedalus 0.2.5. No new U1 capture,
watcher, provenance ring or diagnostic option is included.

## Lost CPU stores during readback

The previous DownloadMemory shadow merge saved CPU-owned bytes, wrote the GPU
range back, then restored the saved bytes. A CPU store after that save could
be overwritten by restoration. This is a concrete data-loss race independent
of whether the game immediately crashes. It is consistent with U1's corrupted
LightProbesJob input, but the original log alone does not prove it was that
particular writer or the sole cause of the intermittent crash.

The merge now normalizes CPU-owned ranges and excludes them from GPU writeback.
Their live storage stays in place; no saved CPU snapshot is restored. GPU-owned
parts receive the same completed GPU data as before. No new producer wait,
readback precision override, side queue override or job-scheduler change is used.
This also removes the allocation/copy of preserved CPU payloads. The existing
U2 asynchronous snapshots, budgets and data-before-label gates remain intact.

A deterministic regression test reproduces a later CPU store being lost by the
old save/write/restore sequence, then proves it survives the new production
range writer. 5,000 overlapping/random range cases check that every GPU-owned
byte and no CPU-owned byte is written. On-device crash validation remains pending.

## U3 startup logos

The existing UI preference has catalog recipes for U1/U2, but none for U3.
GameActivity now applies the existing preference to a guarded U3-native hook on
every boot/loadexec. U1/U2 retain their catalog recipes.

Three unique executable signatures identify successful movie initialization,
the metadata field inside that initializer, and the player's original Stop(1)
virtual call used by skip input. The corrected thunk executes from the update epilogue only at playback state 5 and
calls that same Stop(1) only for exact names `bp-logos` or `sce-ndi-logos`.
The normal state update, stop/decoder teardown and common epilogue remain intact.
Story movies and prefix-lookalikes are not stopped. Comparisons stop at the
first differing byte/NUL, including short names at a protected-page boundary.

U3 SELF SHA256 acc952e5a9ac7062f8ecfc52700faa599d5f29deb1e24f9e81e6de832326d1d0
was used for binary verification. Its entry 0x4897a0 matches the supplied U3 log.
All three signatures match once, at RVAs 0x4e4137/0x4e36c2/0x1eae47.
Targets and metadata offsets are derived, not guessed from another version.
Missing/ambiguous signatures or failed ownership/bounds checks leave code alone.
Other versions are eligible only if the complete signatures and checks fit;
only this supplied executable has been verified against real bytes.

Tests execute the emitted guest thunk with the two logos, ordinary movies,
lookalikes, empty/null names and guarded-page short strings. The actual installer
and signature scanner are also executed against relocated verified fixtures.
The original one-shot post-initialization hook was reported ineffective. It has
been replaced by a playback-gated check in the common update epilogue, replaying
the displaced guard load/compare before resuming the original conditional branch.
Idle, decoder initialization and teardown do not read movie metadata or stop it.
The corrected behavior still requires on-device startup validation.

## U3 shadow trails isolated to resolution patches

The user isolated the trails to reduced-resolution patches, with effects enabled.
The supplied U3 SELF is identical to the previously analyzed one (SHA above).
Its 1.00 catalog patches changed scene dimensions (+0x08/+0x0c, +0x18/+0x1c,
+0x38/+0x3c) and the tile grid in both display-configuration initializers, but
left +0x24/+0x28 at 1920x1080. The main 1.02 recipe already changes this pair.

The executable confirms UpdatePostProcessingCsConst (0x98b150, named by its own
assert), its tile-grid constants (0x98b310), and other post passes read +0x24/+0x28.
That left reciprocal pixel sizes and tile counts inconsistent with reduced targets.
For example, 360p used 640x360 surfaces but 1920x1080 post-processing dimensions.

All four 1.00 U3 resolution batches now also change +0x24/+0x28 in BOTH
initializers (immediate addresses 0x902fa9/0x902fb0 and 0x9035e0/0x9035e7).
All four 1.02 catalog fallback batches include the equivalent fields already in
the 1.02 main recipe (0x12396e0/0x12396e7). Existing movie allocation rewrites and
fixed native movie-blit dimensions remain unchanged. Host presentation/FSR output
policy is unchanged. No effects, shadow cascades or global GPU policies change.

Every 1.00 write, including existing movie code, was checked against the supplied
original SELF. Tests check all eight resolution batches, both constructors,
non-overlap and fixed movie surfaces. This fixes the demonstrated dimension
mismatch; visual confirmation that all reported trails disappear is still pending.
The earlier XML parity report describes the catalog BEFORE this correction.

## FSR and native AA: confirmed interaction

The user confirmed FSR works only with the native anti-aliasing disable patch;
turning native AA on while FSR is selected breaks rendering. Preset generation
now automatically selects the existing verified AA-disable recipe for any FSR
mode, including FSR Native AA. It never changes the user's saved AA preference.
With FSR off the saved preference applies again. The AA row shows FSR and is
managed while a FSR mode is selected, updating immediately on preset changes.

FSR startup requires both a compatible resolution recipe and a successfully
verified/applied AA-disable batch. Failure/ambiguity/overlap does not start an
incompatible native-AA-plus-FSR combination; it uses the existing unsupported
preset fallback instead. No AA patch offsets, FSR renderer, cascade cap, camera
motion mode or unrelated effect preference was changed.

This directly addresses the user-confirmed AA/FSR conflict. Whether the reported
shadow trails also disappear still requires a visual check; the previous log
alone does not prove all trails came from this interaction.
