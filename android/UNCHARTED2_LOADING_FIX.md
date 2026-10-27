# Uncharted 2 streamed-data and mesh-result corrections

Evidence: `Uncharted-Daedalus-20261027-181100.log.txt` ends in the game's assertion
`iSubMesh < m_numSubMeshInstances`, GetSubMeshInstance, mesh.h:1361, after checkpoint
save workers. This is a distinct captured failure from the earlier null access.
The same assertion is documented in upstream PC issue #695.

## File reads into tracked pages

The Android runtime still read directly into write-protected guest destinations.
Its touch/retry loop races with GPU protection: kernel read/pread returns short
or EFAULT without running the guest page-fault handler. After retries the caller
can receive incomplete streamed assets. Upstream explicitly associates this
case with the U2 submesh assertion.

Port the host-buffer approach from Supermedo/uncharted_pc commit c30d6119cc0f3c21deda8ecaf3a00e8834b01ef7.
Read into host memory, then copy in user space so tracked-page faults can resolve.
Unlike the upstream 4 MiB pointer retained for each thread, use 256 KiB bounded
scratch storage with a pthread-key destructor. EOF, EINTR, offsets and partial
real I/O errors retain their normal semantics. Windows keeps its existing path.

The deterministic regression reapplies destination protection immediately before
each kernel read, reproducing the actual race window. The old production code
returns truncated data. The corrected production function passes full read and
pread across multiple chunks, EOF, zero size, file offsets and EBADF cases.

## CPU-visible mesh raycaster records

Upstream vk_shader_hle.cpp explains another cause of this exact assertion after
checkpoints: the U2 64-dword copy shader 0x6a6422a9 copies GPU-generated raycaster
results to arrays later read by MeshRaycaster::HandleResults on the CPU. GPU-only
copy leaves the CPU's records stale under the existing memory policy.

Enable only the mode-2 correction for U2: CPU-owned result destinations receive
the actual source bytes, synchronizing GPU-written sources when needed. GPU-only
destinations and the 1-dword copy shader keep their existing policy. U1/U3 do not
enable this mode. BB_GPU_MERGE and global readback policy are not changed.

This base lacks upstream's complete deferred landing/gate mechanism. Consequently
these U2 64-dword copies drain the draw pipe, and the targeted source read occurs
on the GPU command thread before later SRT walks. Blocking on the recorder itself
would risk a self-wait. The targeted synchronization may cost performance; this
build does not claim the deferred implementation or a measured FPS gain.

Sources:
- https://github.com/Supermedo/uncharted_pc/issues/695
- https://github.com/Supermedo/uncharted_pc/commit/c30d6119cc0f3c21deda8ecaf3a00e8834b01ef7
- https://github.com/Supermedo/uncharted_pc/blob/754ed2ab4f1fdd21250c8785d4d4f49517d31292/gpu/shadps4/video_core/renderer_vulkan/vk_shader_hle.cpp
- https://github.com/Supermedo/uncharted_pc/blob/754ed2ab4f1fdd21250c8785d4d4f49517d31292/run.py

No assertion is removed. Resolution/FSR, shaders, CPU clock/FPS limits, audio,
fonts and the three optional optimization controls remain as before. The earlier
crash-only live FEX capture remains available if the null-access crash recurs.
App version/package/signing identity remain 0.2.5. Compilation/regression checks
do not establish that both observed gameplay crashes are eliminated on a phone.
