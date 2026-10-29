# Resolution recipes

One JSON per game (`u1.json`, `u2.json`, `u3.json`), shipped in the APK as assets. The launcher
makes the game's byte patches from them and the user's own prepared executable
(`Launcher.java makePreset`), for the size chosen in its settings: 640x360, 960x540, 1280x720 or
the game's own 1920x1080. Made by `../recipe_from_preset.py` from a BBPATCH2 preset and the
executable's image; `--check 1280x720` proves the round trip.

A recipe holds no bytes of the game. For every write of the patch: the address (PS4 virtual,
base 0x400000), the length, the SHA-256 of the executable's original bytes there (the launcher
applies the recipe only when they match: the writes are for version 01.02 of the executables),
and only what changes:

- `fields`: little-endian integer fields that hold the resolution, as formulas - `W`, `H`
  (1920x1080 in the game's display configuration: five size pairs) and `W/8`, `H/4` (the 240x270
  light tile grid). The user's size goes in; the same bytes come out at 1280x720 as the presets
  the port ran with until 2026-10-23.
- `edits`: the other differing byte runs, as they are: new instruction bytes of our own patches
  (`../u1_extra_patches.xml`, `../u23_extra_patches.xml`: the movie chroma plane, the job workers'
  sleep mode, the navmesh fixes) and of the community's movie texture lines (the Bachata S4
  catalog). The bytes around them come from the user's file.

`when`: `below_1080p` for the resolution fields and the movie fixes (at 1920x1080 the game needs
neither), `always` for the rest (the low-FPS assert bypass, the logo skip, the sleep mode, the
navmesh fixes), which the launcher applies at every size, 1920x1080 included.

## Versioned Bachata catalog

`bachata.json` retains separate 01.00 and 01.02 batches for all three games.
The four fixed resolution choices are 720p, 540p, 468p and 360p. Each complete
batch is accepted only when all original-byte SHA-256 checks match. The launcher
requires exactly one matching variant and rejects overlapping writes; SFO labels
do not override executable compatibility. The existing recipe takes precedence
where it matches, and the versioned catalog provides the fallback for 01.00.

The supplied corrected XML was compared to all 112 catalog entries without
removing the 01.02 entries. All 12 resolution variants for the three supplied
01.00 executables match their original bytes. 01.02 executables were not supplied
for direct comparison. See `android/validation/patches-1.00-1.02.json`.

U3 correction after device testing: reduced-resolution catalog batches now also
scale the display-configuration +0x24/+0x28 pair, matching scene targets and the
main 1.02 recipe. Both 1.00 constructors are covered. Movie textures/blit remain
native. See `android/STABILITY_U3_PATCHES.md`; the earlier XML parity report is a
historical validation of the supplied XML, preceding this correction.
