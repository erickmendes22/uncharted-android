# Optional optimizations on the published Daedalus 0.2.5 base

Base: `75f143948f4ea94ee62a20ca28267e4e7543df1d`, GitHub Actions `40389005820`.
Its APK's SHA-256 is `2765597935be2df6adc2633adce22d050b59fc80237bb0e04c1152bf82db61a3`,
verified against the published 0.2.5 release asset. The GitHub release tag points
to older source with different UI; this verified artifact identifies the actual
source of the installed release.

The S25 investigation is excluded. Audio, font/HUD loading, log export and CMake
are byte-identical to this base. Version/package/signing identity are unchanged.

## Corrected scope

- Optional mapped GPU reads for U2/U3: copy only data whose GPU work has already
  completed. No new producer wait and no forced precise readback. Incomplete,
  shadowed or unmapped data uses the original memory policy. Readback merging
  and the side queue retain the original release's settings.
- Optional submission after 100 draws/dispatches, independently selectable on U2/U3.
- Optional idle-worker sleep/semaphore hooks on validated executable patterns.
  The pinned FEX uses a private INT 0x82 trap for these hooks. Its original
  SYSCALL behavior is unchanged even when worker hooks are enabled.

All three controls default OFF. Existing explicit choices are preserved. OFF
leaves every new mechanism disabled, including the native worker-hook default.
At every native startup, old generated per-game compatibility overrides are
discarded, including the previous APK's `BB_GPU_MERGE=0` and `BB_SIDE_DOWNLOAD=0`.
Global manual overrides, saves, game files, resolution, drivers and language are
preserved. Cleanup also works if GameActivity starts without visiting the launcher.

The previous integration incorrectly kept precise readback and the side queue
disabled when the mapped-read switch was OFF. It also used the S25 recovery base
and changed FEX generic SYSCALL register behavior globally. None of those choices
is retained here.

## Validation

Tests cover all 24 game/switch combinations, stale generated config cleanup,
explicit OFF output, native worker OFF by default, loop validation and callback
register preservation, batching limits, unchanged release files, absence of S25
files, absence of extra producer waits, and isolation of the FEX trap.

Device performance and stability still require testing. No FPS gain or device
crash resolution is claimed from compilation or unit tests alone.
