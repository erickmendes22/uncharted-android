# Uncharted Daedalus 0.2.5 crash-path fixes

## Evidence and limits

The supplied Uncharted 3 log reports an unsupported ImGui font, an AAudio
disconnection followed by audio reinitialization, successful FSR temporal
dispatches at 1280x720, and eventual background presentation suspension. It
contains no terminating native stack, Java exception, or low-memory kill reason.
The precise cause of the reported device crash remains unconfirmed.

## Changes

- Pin each audio port under the table lock before releasing handle lookup.
  Closing or reopening a port can no longer invalidate a stream between lookup
  and acquisition of its mutex. State, volume and output-time queries use the
  same lifetime rule.
- Initialize port mutexes once for the process lifetime; reopening clears only
  the port payload, never a live mutex. Shared audio counters are atomic.
- Validate font container/table bounds and stb compatibility before adding a
  system font to ImGui. Unsupported CFF2 sources are skipped without mutating the
  atlas. Preserve the embedded fallback and load at most one compatible CJK
  source instead of multiple overlapping large fonts.
- Export Android's previous-process exit classification, timestamp, status and
  RSS/PSS alongside the existing session log. This runs only on export and is
  labeled as a previous process, not as a diagnosis of the current session.

No renderer synchronization or FSR behavior changes are included. Version
remains 0.2.5.

## Validation

- Deterministic audio lifetime invariant: pre-fix code fails; fixed code passes.
- 500 audio close/reopen cycles with concurrent output, state, volume and time
  queries plus foreground transitions, under AddressSanitizer/UndefinedBehaviorSanitizer.
- Invalid, truncated, out-of-bounds and CFF2 font inputs rejected without adding
  atlas sources; real TTF, synthetic TTC, fallback drawing and FPS arrow text pass.
- Full Android compilation and existing regression checks run in CI.
- Gameplay crash resolution still needs confirmation on the affected phone.
