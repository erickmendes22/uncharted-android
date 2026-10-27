# S25 Ultra: isolate draw scheduling

Base: 8a4bae6b79399c449e5372c48933cd9799080546 (Uncharted Daedalus 0.2.5).

The supplied videos show unstable game-menu composition. The touch overlay remains
legible. These observations do not prove that FSR, the driver or the display
transform causes the problem. The supplied log does not contain a fatal trace.

Earlier logs use the same V37 driver, four preparation workers and coherent
constant-ring memory. Their display is 1216x2688 at 60 Hz; the supplied log uses
1080x2340 at 120 Hz. They are different sessions/builds and different games, so
these differences are not a controlled comparison and do not establish causality.

This diagnostic build forces only BB_DRAW_PIPE=0 after loading environment files.
The speculative shader-preparation workers, driver, FSR settings, resolution
patches, frame cap and display settings are preserved. The existing serial draw
path is used; it still has ordinary Vulkan command recording and GPU execution.
Lower performance is possible. No permanent Samsung override is introduced.

The exported session log identifies the diagnostic profile and device at startup.
If the same menus become correct, narrow the next investigation to draw-packet
snapshots, constant-ring lifetime and deferred guest-visible fence/copy ordering.
If they remain broken, disabling this queue is not sufficient; do not merge it
as a fix or disable unrelated optimizations without evidence.

No on-device result is available at build time. This is a diagnostic, not a
confirmed graphics fix. The release branch and normal APK remain unchanged.
