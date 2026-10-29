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
virtual call used by skip input. The new thunk executes AFTER initialization and
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
On-device startup validation remains pending.

## U3 shadow trails: corrected scope

The user confirmed trails also occur with all effects enabled. Effect-removal
patches therefore do not explain the reported issue. The proposed source/frame
cascade-count change was removed before publication; existing shadow patches
remain unchanged. Investigate scene/depth/velocity history and GPU resource
coherency without claiming that FSR is executing merely because it is available.

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
preset fallback instead. No patch offsets, FSR renderer, cascade cap, camera
motion mode or unrelated effect preference was changed.

This directly addresses the user-confirmed AA/FSR conflict. Whether the reported
shadow trails also disappear still requires a visual check; the previous log
alone does not prove all trails came from this interaction.
