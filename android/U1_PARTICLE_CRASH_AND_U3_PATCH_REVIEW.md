# U1 particle crash and U3 patch review

## Confirmed U1 failure

Input: Uncharted-Daedalus-20261028-235913.log.txt. The supplied SELF
`eboot(20261026-110841).bin` has SHA256
8953b6ed5cf4d1d26aa3aa4cd1e0c72817ea155f6a5774fb17f6fec7d787f953.
Its entry (0x350600), fault instruction bytes and caller return addresses match
this log. Guest image base is 0x400000; XML addresses are not flat SELF offsets.

At guest PC 0x8ed8da, `popcnt rcx,[rax]` reads RAX=bfaaba3ec0a0c35e.
The immediately preceding instruction loads RAX from job+0x10. Job address is
0x1014370330. This is `LightProbesJob`, identified by its own executable strings
and cross-references to particle/ngen/particle-light-probes.cpp. It counts a
1024-bit particle mask and builds records for selected particle light probes.

The entry wrapper (0x8ede60) reads input=[job+8], then copies [input] to
[job+0x10], [input+8] to [job+0x18], a flag byte to +0x20 and [input+0x28]
to +0x28. The constructor (0x8ed830) stores the global/input pointers. Its sole
launch site constructs a 64-byte job and submits it via 0x752ed0, with an
existing counter wait at 0x8d2403. Blindly inserting another synchronous wait
or accepting negative counters is therefore not justified by this log.

The invalid mask pointer is in the task's input chain. The log does not contain
the input pointer, constructor snapshot or write provenance, so it does NOT
identify which CPU/GPU writer damaged or recycled it. A noncanonical pointer
alone does not prove OOM, a driver fault, FEX POPCNT mistranslation or shaders.

## Targeted provenance build

Only the exact U1 constructor, entry and fault signatures above enable capture.
Other executable versions and U2/U3 do not install it. Two original prologues are
relocated using the loader's existing register/flag-preserving callback thunk;
the constructor's original RDX is passed explicitly as the source header pointer.
All displaced instructions execute normally. No recovery, data substitution,
skipped task, readback mode or scheduling change is introduced.

A 256-job history records source headers at construction, with a bounded 4096
successful backing-write history (~140 KiB total). Invalid input at entry emits
at most two reports; the precise fatal instruction also emits the retained
constructor, current input, copied job mask and overlapping backing writes.
No background polling, filesystem dumps or per-frame output is added. Captures
use safe self-process reads, with an already-open /proc/self/mem fallback.

This distinguishes an input already invalid at construction, later source
mutation, changed job pointer/copy, and recent overlapping backing writes. The
history explicitly cannot exclude older writes or inline guest CPU stores.
No claim of a gameplay fix is made before this provenance is captured.

## U3: skip startup logos

Input: Uncharted-Daedalus-20261029-091957.log.txt. The supplied
big3-ps4_Shipping(1).elf SELF has SHA256
acc952e5a9ac7062f8ecfc52700faa599d5f29deb1e24f9e81e6de832326d1d0;
entry 0x4897a0 matches the log. The UI stores patch_Skip startup logos, but
recipes/bachata.json has no such entry for U3 1.00 OR 1.02. Its presence for
U1/U2 cannot apply to another executable. This explains the inert U3 option.
No U1/U2 offsets were copied to U3. A safe U3-specific skip path is still pending.
The U3 input/state branch at XML address 0x5eae4d was inspected but is not proven
to affect logos exclusively; forcing it could also skip unrelated movies.

## U3: shadow trails

The logged 35 writes / 522 bytes exactly match the 1.00 catalog's 360p resolution
(21 writes / 353 bytes), ONE reduced-cascade patch (1 / 17), and ALL twelve
remaining effect-removal options (13 / 152). This aggregate cannot distinguish
one vs two cascades. It supplies no visual A/B result or per-effect manifest.

The cascade patch caps the local cascaded-shadow count (original min(count,4),
patched min(count,2) or min(count,1)); its original 17 bytes and SHA match. The
motion-blur, SSAO, AA, dynamic-light and sunlight-occlusion patch original bytes
also match. Matching bytes establishes binary compatibility, not correct visual
behavior or causality for trails. Motion blur skips a combine block; sunlight
occlusion changes a shader constant. Neither has been proven the cause here.
FSR is available/enabled and camera-only motion is configured, but the log does
not establish actual FSR dispatches or shadow history correctness. No unsafe
renderer rollback or automatic removal of unrelated user-selected effects was
made from this aggregate log alone.

## Validation

The provenance test executes the actual production thunk on x86-64: original RDX
is captured, displaced stores remain correct, a valid source becomes invalid
between construction/entry, and the constructor/current values plus overlapping
backing write are reported. Invalid addresses fail safely and ring wrap is bounded.
Existing shader deadlock, SRT recovery, landing order and release-base tests pass.
On-device provenance and visual validation remain pending.
