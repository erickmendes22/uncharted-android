# Uncharted Daedalus 0.2.5 integration review

## Source snapshots

- Existing Daedalus baseline: `b6d1c8cba5f6780421b87be9c8cc870e2a323967`.
- Original Android release: `revvv991/uncharted-android`, tag `v0.2.5`, `2d20740`.
- Bloodborne Android: `ducvd89/Bloodborne-Android`, master `9ca44ac`.
  Mobile performance/memory policy originally introduced by `776f6972`.

## Compatibility decisions

| Source / candidate | Decision | Integration |
| --- | --- | --- |
| Android 0.2.5 post-cutscene frame slots / late-frame pacing | Included | Clamp next slot to 15/16–1 frame after presentation; a 60 Hz movie cannot accumulate 30 FPS timing debt. |
| Android 0.2.5 texture-cache LRU starvation | Included | Ineligible entries no longer consume deletion slots; visits remain bounded. |
| Android 0.2.5 warm off-screen textures | Included | Normal reclamation uses 20 seconds instead of a few submissions, reducing re-uploads. |
| Android 0.2.5 CPU dirty-image hashes | Included | Mark/refresh hash the same guest range; GPU-writable mip baselines prevent stale CPU reuploads. |
| Android 0.2.5 depth/stencil lifetime fix | Included | Reacquire image references after the slot vector can grow. |
| Android 0.2.5 recorder handoff | Included | Acquire the replacement before moving the recording chunk into its queue. |
| Android 0.2.5 bundled A735 command-page workaround | Not included | Specific Mesa-driver change; not needed to replace the functioning imported 8xx drivers. Requires a separate pinned-driver rebuild/validation. |
| Android 0.2.5 PC 0.3.9 merge (background pipelines, shader/barrier, cache/VRAM changes) | Reviewed, not merged wholesale | Larger interdependent renderer/compiler changes are not assumed safe on the working mobile FSR path. |
| Bloodborne adaptive memory budget | Included, adapted | Starts at 1–4 GiB from actual MemAvailable when the texture collector first runs, reserving 1.5 GiB for growth. Watches RAM/swap/PSI every 2 s; usage floor, gentle recovery and short-memory idle reclamation retained. Overrides remain available. |
| Bloodborne ADPF | Included, adapted | Registers main/GPU/DrawScan/DrawPrep/recording/guest render threads; CPU work excludes limiter sleep. Unlimited mode targets the display refresh rather than a fixed 60 FPS. Missing APIs disable the hints safely. Thermal tracing is off by default. |
| Bloodborne Android game classification and GameState | Included | Category/isGame, Game Mode metadata, loading/playing/background state, no forced OS downscale/FPS override. First flip reports playing once. |
| Bloodborne movie LSFG pause | Reviewed, not copied directly | Uses AvPlayer state specific to Bloodborne; Uncharted uses Videodec2 and its existing no-scene reset path. |
| Bloodborne AOT recompilation / native guest CPU | Not transferable directly | Its libraries and generator cover Bloodborne's executable/ABI; this build retains Uncharted's working FEX backend. |
| Bloodborne opt-in UMA/hot-pages/texture-home experiments | Not enabled | Changes the guest-memory ownership model; not a release default without Uncharted-specific GPU validation. |
| Bloodborne default 1080p output | Not adopted | User-selected Uncharted FSR output remains 720p. |

## Product changes

- Public name: Uncharted Daedalus, version 0.2.5. Same package/signing identity.
- Public version code 100205 permits installation over development code 210.
- About follows the inFAMOUS structure: version/runtime, fork/base credits, project links,
  channel/repository links, licenses and control help.
- Driver descriptions are assembled from translated labels; driver names/versions/authors
  remain unchanged. About and log-export UI translated in all 12 offered UI languages.
- Removed the unused More tab. Video/HUD/Controls and frame generation via own DLL remain.
- Release default disables FPS/mapping and thermal diagnostic tracing; errors/startup details
  and log export remain available. Existing advanced overrides are preserved.
- The 1.00/1.02 patch catalogs, 720p FSR modes and touch/HUD placement behavior are preserved.

## Validation scope

Host tests cover memory/swap/PSI thresholds, active-usage floor, slow recovery/cap,
post-movie pacing bounds, existing patch signatures, language IDs, touch layout and UI.
CI compiles the ARM64 application and verifies the APK identity, version, signature and
packaged resources. These checks do not establish an FPS gain or device gameplay stability.
Compare gameplay and cutscenes on the phone, with the same driver and settings, before
claiming a measured performance improvement.
