# Persistent graphics-state investigation

Base: 8a4bae6b79399c449e5372c48933cd9799080546, version 0.2.5.

The S25 logs identify SM-S938B, Android 17 / SDK 37, V37, and the
s25-serial-draw diagnostic. The user reports that changing patches, resolution
or FSR triggers corruption that survives reverting visible choices and restarting.
Disabling the two-stage draw queue was therefore not sufficient.

The launcher previously set upscaler=off and preset=0 when selecting manual mode
but retained output_res from the previous configuration. Manual startup now writes
the port's canonical 1280x720 output setting as well. This is a consistency fix;
it does not establish the cause of the reported corruption.

The new optional Restore graphics defaults action backs up the old INI, launch
arguments, environment files and graphics preferences before resetting them. It
restores manual 720p, no optional patches, FSR/FG off and 60 FPS, and removes hidden
renderer overrides. It retains saves, prepared executables, drivers, language,
controller/HUD settings and fullscreen. Unrelated advanced overrides are retained.
No reset runs automatically on installation or startup.

A bounded, startup-only graphics snapshot records launcher patch choices, INI
graphics values, the effective renderer environment and the selected patch-file
checksum. It is prepended to exported logs. It does not log arbitrary environment
variables or add per-frame instrumentation. This distinguishes a visible setting
from what the next process actually loaded.

The normal draw pipeline is restored: this branch is based on the release source,
not the previous serial diagnostic. There is no forced Samsung renderer override.

TestGraphicsProfile checks hidden-state reset, backup fidelity, preservation of
unrelated settings, scoped environment cleanup and snapshot contents. Android
compilation and existing regression checks run in CI. A device result is still
required; this build must not be described as a confirmed graphics/crash fix.
