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
