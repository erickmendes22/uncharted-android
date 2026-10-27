# Uncharted 2 crash investigation

Input: `Uncharted-Daedalus-20261027-173021.log.txt`.
Base: `d2bc15f5ff81a8997f5d4a269b785a38b3d43d2e`, the verified release-base integration.

The fatal event is SIGSEGV at host PC `0x6a627365b8`, fault address `0x20`, on
`NdJobWorkerThre`. The PC lies in `[anon:FEXMemJIT]`. The existing report prints
the last synchronized CPUState at host veneer `0x75d3422398`; the next veneer
loads ID 0x38, so that state belongs to an earlier HLE call, not the failing guest
instruction. The live ARM registers map to a different RSP (`0x12011fbda0` instead
of `0x12011fbee8`), with RAX/RBX/R14 zero at the fault. Those are clues, not proof
of which object or instruction failed.

The crash follows a movie decoder reset and subsequent output, but temporal
proximity alone does not establish a decoder defect. FSR is 640x360 -> 1280x720;
mapped reads are enabled; idle-worker hooks did not match this executable.
There is no terminating GPU hang/device-lost/OOM message in this log. The
startup font warning is not a terminating stack trace.

## Changes

- Capture the interrupted host PC via the pinned FEX's existing
  `RestoreRIPFromHostPC` API, also used by its signal-based profiler.
- Print the live Linux ARM64 SRA register values, current guest and ARM JIT code
  bytes, a bounded live frame walk and fiber bounds, only on a fatal signal.
- Include the last completed runtime call's name and returned value. Recording
  this adds two thread-local stores per HLE return; it does not sample, log calls
  continuously, or allocate a trace ring.
- Keep the old synchronization snapshot separately for comparison.

No game instructions are skipped, no NULL page is mapped, and no fault is
suppressed. Renderer, readback, frame pacing, patches, audio and fonts remain
unchanged. This is a crash-capture build, not a claimed gameplay crash fix.

The existing local U2 executable is a different binary from the one in the log:
its matching camera signature is at a different offset. Therefore the stale
stack values must not be used as offsets for a speculative game patch.

## Validation and next evidence

The pure register-mapping test reproduces the uploaded log's stale/live stack
difference and verifies that capture does not mutate the saved guest state.
The release-base/readback contract remains checked. Full Android compilation
and APK checks run in Actions. A fresh crash log from this APK is required to
identify the actual x86 instruction and its current call frames before fixing
the underlying null access.
