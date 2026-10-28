# Gameplay freeze: speculative compiler fault handling

Base: 59434aa93b9b2aad27a6f2bac13cdd85d1c766fe, app 0.2.5.

The supplied U2 log records renderer/cache features enabled and no fatal signal.
It ends at application backgrounding. The user confirms an unexpected freeze
mid-game; absence of a fatal signal is not proof of continued gameplay.

## Concrete blocking path repaired

`probe.c::fault` dispatches GPU access-violation handlers before its speculative
recovery branch. `PageManager::GuestFaultSignalHandler` can resolve a protected
GPU-mapped read by queuing `BufferCache::ReadMemory` to Liverpool and waiting.
The GPU command owner can meanwhile be waiting for a program/pipeline claim
held by the same speculative compiler worker. This forms an owner/worker cycle.
Unlike the earlier look-up-only workers, compilation workers can hold those
claims while reading guest tables.

`bbgpu_handle_fault` now declines GPU fault handling only when the calling
thread is a background helper AND it already has a speculative recovery point.
The existing loader then resumes that recovery point. Real guest/recording/
GPU-owner tracked memory faults retain their previous handling; readback
precision, side queue and guest data synchronization are unchanged.

Graphics selection now has recovery around its initial GetParams/table reads.
Nested program translation/lookup restores the previous recovery pointer rather
than erasing an outer point. Vertex-input descriptor preparation also falls
back to the regular GPU-side binding path on a stale speculative pointer.
No incomplete vertex binding or newly failed selection is published as usable.

## Validation and limit

`test_speculative_fault_dispatch.py` compiles the actual production dispatcher
against a deterministic waiting-owner model. Removing the new guard reproduces
the circular wait; the patched dispatcher completes without entering owner
work. Ordinary tracked faults are checked too.

`test_shader_selection_recovery.py` executes the production graphics-selection
method with failing guest reads before and after nested lookups. No pipeline
is published and recovery pointers are restored. Existing ARM interpreter
recovery, U2 result landing order and release-base guards also pass.

The phone's supplied log does not contain the frozen threads' stacks. This is
a reproduced blocking bug and a targeted correction, not confirmation that it
is the sole cause of that phone session. Device gameplay validation remains
required. No unrelated font/FSR, new per-frame logging, readback policy or
performance-default changes are part of this fix.
