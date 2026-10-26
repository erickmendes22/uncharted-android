# Uncharted 0.2.6 — source and patch validation

The user authorized a test build on 2026-10-26 after the XML/executable checks.
The build hold was removed; runtime and device validation remain pending.

| Request | Source changes |
| --- | --- |
| Mobile controls | Semantic action layout, right-side camera, classic fallback, persisted switch |
| Movies with resolution patches | Movie scaling follows the registered display dimensions, rather than blindly using requested render size |
| App icon | Official Nathan Drake Collection image, packaged as an Android drawable |
| Stronger Uncharted UI | Official hero artwork, adventure selection/Play focus, installation/preparation moved into Settings, automatic preparation on Play |
| FPS HUD | Minimal/compact/detailed views, position/size controls, process CPU/RSS and KGSL busy readings; unavailable GPU data shown as -- |
| Upscale presets | Explicit FSR 3.1 Native/Quality/Balanced/Performance/Ultra selection; native 1080p guest/display, existing reduced scene proxies before temporal reconstruction, separate manual resolution mode |
| Stability | Adapted community NDGI/job-wait hooks to FEX veneers and bounded waits, Linux low thunk allocation; original command-buffer assertions retained |
| Log destination | Android document picker from launcher and in-game, streamed export to user-selected destination |
| PKG install/update | Adapted ISS installer, fresh Collection subdirectory, base/update title validation, pinned orbis native helper |
| Exit game | Save settings, request task removal, end game process; no return-to-launcher loop |
| Log naming | uncharted.log internally; Uncharted-YYYYMMDD-HHmmss.log on export, with legacy log reading for existing installations |

## Evidence already examined

- Uploaded SELF SHA-256: 8953b6ed5cf4d1d26aa3aa4cd1e0c72817ea155f6a5774fb17f6fec7d787f953.
- Supplied log: command-buffer ownership/in-use assertions, not an observed OOM.
- The log reports 1280x720 movie content in 1920x1080 display buffers.
- The uploaded image has unique NDGI begin/end sites at image offsets 0x40c7e7
  and 0x40c91f, and the community job-wait pattern at 0x353353.
- Stability reference: drakolordx7/uncharted_pc community branch, Mevlock job-wait
  integration; upstream issue Supermedo/uncharted_pc#620. Android keeps other
  workers yielding normally; only an open render context waits in place, bounded
  to 250 ms by default, before the original suspend path.
- Artwork source: official PlayStation Collection product page. Download URLs
  and SHA-256 checks are pinned in scripts/fetch_art.sh.
- PKG helper: videobitva/orbis@13f1294a87a53d54f82feec56acbfd03f50667a9.

## Checks and limits

Java source parsing, Android manifest parsing, C syntax, shell syntax and diff
whitespace checks pass. At the source-validation checkpoint there had been no Android compilation/linking,
new APK or device validation of these changes. In particular, FSR image quality/history,
video sizing, PKG extraction and crash recovery still require the later approved
build and device test. Do not report those runtime results as confirmed.

## Corrected XML validation — 2026-10-26

The supplied corrected XML matches all 112 existing catalog batches exactly:
56 for 01.00 and 56 for 01.02. No variant was removed, retagged or duplicated.
The catalog now records the source XML hash and supported versions.

All 12 resolution batches for 01.00 (720p, 540p, 468p and 360p across U1/U2/U3)
match the supplied executables. The other 44 optional 01.00 batches also match
Expected bytes; this is compatibility evidence, not validation of their effects.
All 96 movie-call rewrites across both versions retain the original call targets;
replacement lengths, instruction decoding and push/pop balance were checked.
The 01.02 batches remain identical to the XML, but 01.02 executables were not
supplied for direct comparison. Runtime selects an unambiguous variant using
original-byte hashes instead of trusting an SFO version label alone.

Reproduce with `scripts/g2/validate_catalog.py` (under android/) and `--xml`,
`--catalog`, one `--exe canonical-name=path` per executable, and `--report`.
See `validation/patches-1.00-1.02.json` (under android/) for hashes and counts.
No build was generated during patch validation. A subsequent test build was
authorized by the user on 2026-10-26.
