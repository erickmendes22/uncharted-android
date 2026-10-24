# Patches

- `fex-*.patch`: FEX (`~/src/fex`, branch `g2-android` on top of FEX `f2b679f`), MIT license:
  0001: the Android build of FEXCore and Common (bionic, libc++), a FEXCore-only target; 0002: the
  guest's pause is a `wfe` (`BB_PAUSE=yield`: a yield); 0003: that target without a smoke executable.
- `mesa-*.patch`: Mesa/Turnip (`~/src/mesa`, branch `g2-kgsl` on top of Mesa 26.2.5), MIT license:
  0001: polling timestamps instead of waiting with a zero timeout (fixed the same way upstream,
  e984ef294ea); 0002: u_trace gate; 0003: diagnostics: `TU_BO_LOG=1` (BO log with addresses,
  together with `TU_DEBUG=bos`), `TU_KGSL_CTX_FLAGS` (KGSL context flags); 0004: diagnostics:
  `TU_MEMCHECK=1` (a background thread compares pipeline and shader memory with its contents 2 s
  after creation and prints changes with GPU addresses); 0005: experiment `TU_DS_NO_CROSS=1` (state
  groups do not cross a page; did not help); 0006: experiment `TU_IB_ALIGN=<bytes>`:
  `CP_SET_DRAW_STATE` and `CP_(NON_)CONTEXT_REG_BUNCH` packets in IB1/IB2 do not cross block
  boundaries; 0007: the same for `CP_LOAD_STATE6*`, 1 KB by default; 0008: experiment `TU_BR_SYNC=1`
  (BV waits for BR in sysmem passes); 0009: experiment `TU_CS_FILL_NOP=1` (new command BOs filled with
  `CP_NOP`); 0010: experiment `TU_IB2_SPLIT=<dwords>` (draw IB2s are cut at draw boundaries); 0011:
  alignment off by default again (it only moved the A735 CP faults); 0012: IB2 cut every 256 dwords
  by default (`TU_IB2_SPLIT=0` turns it off); 0013: **recovery after a lost KGSL context**
  (`TU_KGSL_RECOVER`, on by default: after a series of GPU faults the queue moves to a new context
  and the game goes on; check with `TU_KGSL_FAULT_TEST=<s>`).
  Experiments 0004-0011 are off by default; their variables stay for diagnostics.

The port's changes on top of upstream `b787c10` (uncharted_pc 0.3.6), in the order they were made
(in this repository they come as one commit per release). Changes in the code are marked
`g2-arm64:`.

| # | What |
|---|---|
| 0001 | the renderer (libbbgpu) builds for Android arm64 |
| 0002 | the guest<->host boundary (`guest_cpu.h`), native x86-64 backend, host on ARM64 |
| 0003 | the guest CPU backend on FEXCore (`guest_cpu_fex.cpp`), bb-probe for Android |
| 0004 | the app loader (`libmain.so`), Android window, SRT walkers under FEXCore |
| 0005 | Adreno and Android fixes, thread dumps with the guest state |
| 0006 | diagnostics of GPU hangs on Adreno |
| 0007 | more experiments with Adreno hangs |
| 0008 | our own Vulkan driver through libadrenotools (`BB_VK_DRIVER`, Turnip) |
| 0009 | host functions with 9-16 integer arguments; the crash report |
| 0010 | an SRT walker interpreter on ARM64; guest stack margin |
| 0011 | guest rdtsc without scaling; CP idle; the guest profiler (`BB_GUEST_PROF`) |
| 0012 | compute queues without draining the pipeline; their ReleaseMem in order (`BB_WAIT_STATS`) |
| 0013 | bounded MOVREL for known shaders (`BB_MOVREL_RANGE`) |
| 0014 | submits in the middle of a frame (`BB_MIDFRAME_FLUSH`) |
| 0015 | movies at a lower output size (`BB_VDEC_CONTENT`, `BB_VDEC_SIZE`) |
| 0016 | scalar copy down the pipeline (`BB_SCALAR_COPY_PIPELINE`) |
| 0017 | the host's GPU threads on chosen cores (`BB_RENDER_CPUS`) |
| 0018 | compute barrier diagnostics (`BB_CS_BARRIER_STATS`, `BB_CS_NO_BARRIER`) |
| 0019 | splitting big one-dimensional compute groups (`BB_CS_SPLIT`) |
| 0020 | FPS counter (`BB_SHOW_FPS`) |
| 0021 | fence sequence numbers, fewer stage A waits (`BB_IDLE_STATS`, `BB_WAIT_DIAG`) |
| 0022 | batched skinning (`BB_SKIN_BATCH`) |
| 0023 | stage A speedup (SRT block copies, dirty block scan) |
| 0024 | stage B: EOS deferred, `wfe` waits, no dynamic TLS and PLT |
| 0025 | GPU profiler in command stream order |
| 0026 | write overlap check in the scheduler (`BB_RECORD_CHECK`) |
| 0027 | Android pre-rotation: swapchain in the display's orientation |
| 0028 | thread dump on request (`BB_DUMP_REQUEST`) |
| 0029 | GPU profiler: idle between submits by length |
| 0030 | `sceSystemServiceLoadExec` and the port's "restart" restart the app; guest stack in dumps |
| 0031 | PS4 thread priorities below the default -> host nice (`BB_THREAD_NICE`) |
| 0032 | fuller crash reports (guest registers, memory at rbx, fiber), split atomic RMW log, `BB_THREAD_NICE` off by default |
| 0033 | guest traps (`int n`, INTO, privileged instructions) reported (`Guest fault (trap ...)`) instead of ending the guest thread silently |
| 0034 | the recompiler's subgroup size is the one pipelines run with (64): wave64 emulation for 32-lane subgroups no longer runs inside 64-lane ones (ReadLane of lanes 32-63, ballots) |
| 0035 | `Memory:` line every 30 s with `BB_FPS_LOG` (mappings, RSS, swap, available memory, buffer arenas); mapping count in crash reports |
| 0036 | buffer cache guest space 38 bits: the BDA page table 512 -> 128 MiB of device memory |
| 0037 | memory diagnostics: GPU breakdown (VMA, images, arenas), `BB_VMA_DUMP`, `BB_MEM_SMAPS`, mapping alarm, mapping kinds in crash reports |
| 0038 | nested indirect-buffer coroutines destroyed: the compute queue leaked ~32 KiB per IB (U2 crashed out of mappings after ~17 min) |
| 0039 | staging pool on Android: first-fit rings, 64 MiB kept, idle blocks and large buffers freed after ~20 s |
| 0040 | 64 MiB VMA blocks on Android (empty blocks go back to the system sooner) |
| 0041 | U2 skinning with motion vectors (cs 7d7e7206) batched like U1 skinning (`skin_mv_batch.comp`, `BB_SKIN_BATCH_MV=0` off): +4-7% in the palace |
| 0042 | the guest stack protector stop reports where: thread, the calling guest function, registers, stack, fiber |
| 0043 | FEX options from the environment (`FEX_<OPTION>=value` in env.txt, logged at start; FEX had read none) |
| 0044 | skinning batches store interleaved vertices as whole rows through shared memory: cs 8f067540 batch -19% |
| 0045 | helper threads (the Vulkan recording thread, the copy threads) resolve a fault that needs the cache owner themselves while the owner waits for them (`bbport_owner_wait.h`): the U3 chapter 1 freeze (fence labels in pages with GPU data, RecorderFences) |
| 0046 | diagnostics: `BB_REFRESH_LOG` (GPU-written images uploaded again from memory), `BB_POOL_LOG` (image lookups at one address, toggle bit 58) |
| 0047 | constant windows per shader (`BB_RC_WINDOW`; the tiled light 43b8ee5e: 7168 dwords, its lights past the fourth were read from a clamped 256-dword window) and the window clamp on the per-lane part of the offset, so Turnip merges the loads: the light dispatch 110 -> 56 ms/s of GPU time; `BB_FLAT_PEEK` (diagnosis) |
| 0048 | a sharp read through a constant window sits at its offset in the window (per-lane parts as 0), not at the window's start: the tiled light 43b8ee5e bound a null image for its lights' cookie texture, so its lights were black |
| 0049 | shader cache version 11: cached shaders with windowed reads kept the old translation (the cache key did not change) |
| 0050 | T# tables read through a constant window at index * stride (compute) bind as an image array indexed in the shader (`ImageTableIndex`): each light of the tiled light gets its own cookie (0048 gave all of them the first light's); ShaderMetaVersion 9 |
| 0051 | Android app lifecycle in presenting: no presenting in the background (an SDL event watch), a new VkSurfaceKHR for the window Android gives back, acquire timeout 100 ms, no stop on a dead surface: the game survives the screen off (the sleep button) |
| 0052 | Diagnostics: `BB_PEEK_FILE` (guest memory read on request from the running game, process_vm_readv, a reply file), the guest registers of a thread waiting in a host function in the thread dump |
| 0053 | The guest's thread core masks on Linux: logged (the first 64); `BB_AFFINITY_MAP=<host CPU of PS4 core 0>,...` applies them with sched_setaffinity, `BB_AFFINITY_THREADS=<name prefix>` limits that to those threads (off by default; U3 experiments) |
| 0054 | 381ba1d, 90e3030 | `src/game_prepare.c`: the loader's files from the game folder in C (scripts/prepare.py + link_modules.py + content_profile.py, byte for byte the same `boot-linked.bin`/`content.bin`), `libbbprepare.so` (JNI) for the app's launcher |
| 0055 | `BB_AFFINITY_THREADS=<prefix>,<prefix>,-<prefix>...`: several name prefixes and exclusions (U3's A3: every mask but the movie decoder's) |
| 0056 | A CMake warning when libadrenotools is missing on Android (the app could use only the system Vulkan driver); `build_all.sh` builds everything from source |
| 0057 | `BB_AFFINITY_MAP` entries name sets of host CPUs (`3-7`, `0+3-7`): U3's guest threads kept on the big cores (E1) |
| 0058 | `BB_NICE=<name prefix>:<nice>,...`: nice values of guest threads by name (experiments; U3's F1) |
| 0059 | Stage A's drain (GpuComm waiting for the draw recording thread) spins up to `BB_DRAIN_SPIN_US` (50 us), then sleeps until stage B ran its last packet: GpuComm 80-96% -> ~50% of a core, U3's fight 29.2 -> 34.8 FPS |
| 0060 | The texture cache frees an idle overlapping image after 2000 scheduler ticks instead of 32 (~1.5 frames with our submissions): U2's shadow pool was remade every few frames - the palace lamp's flicker (49 -> 2 dips in 40 s); `BB_TC_REMOVAL_TICKS`; the pool log names each free's caller |
| 0061 | Texture cache diagnosis: `BB_ZERO_UPLOAD_LOG=1` reports an image uploaded from guest memory whose first page is all zeros; `BB_COPY_SHADER_LOG=1` also names large scalar copies made on the GPU only (the black-face hunt in U3) |
| 0062 | The GPU thread wakes the guest's submission wait (`WaitGpuIdle`, `num_submits == 0`) only when the last submission retires, not on every one of ~100 a frame: ~100 futex calls a frame and 4300 context switches/s of the guest thread in U3's pub gone |
| 0063 | The userfaultfd tracker reads the kernel's features first and asks for the wanted ones among them (the fixed set failed with EINVAL before 6.4); the G2's GKI kernel has no userfaultfd write-protection at all, so it stays on mprotect, now saying why |
| 0064 | The in-game menu as a touch panel on Android (`gpu/shim/bbport_overlay_android.cpp`): the same settings on the right edge with finger-sized rows of buttons, English, only what applies on Android; a swipe in from the right edge, L3+R3 or Insert opens it, a tap outside, B or Close closes it; the PC window stays elsewhere |
| 0065 | The stream dedupe (0023) is on by default: U2's palace copied 1.37 GB/s of streamed constant buffers into the ring, 80% of them bytes already there; now 0.27 GB/s and the GPU 3 points less busy at the 30 FPS checkpoint, the same FPS in an interleaved A/B; `BB_STREAM_DEDUPE=0` turns it off |
| 0066 | The touch panel scrolls under a finger (SDL finger events; ImGui's own drag did not scroll over buttons) and has a Close button in its header; text 1.8x so it fits 1080p with Advanced closed, the FPS counter toggle in the header, the output size under Advanced |
| 0067 | Hot pages on by default (upstream's opt-in): a page the guest writes again after two faults stays writable and is uploaded on every use instead of faulting and mprotect-ing (TLB shootdowns on every core): U3's pub 27.5 -> 31 FPS, U2's palace unchanged; the set capped at `BB_HOT_PAGES_MAX` (4096 pages); `BB_HOT_PAGES=0` turns it off |
| 0068 | `BB_HOST_NICE=<thread name prefix>=<nice>,...`: the nice level of the port's own threads by name (e.g. `bb:VkRecorder=-10`); changed nothing in U2's palace, a knob for other devices |
| 0069 | The texture cache's 2000-tick removal limit (0060) for depth images only - the shadow pools the lamp needed; colour images back to upstream's 32 ticks (U3 held ~170 MiB more with 0060's rule on all); `BB_TC_REMOVAL_TICKS` (depth), `BB_TC_REMOVAL_TICKS_COLOR` |
| 0070 | The periodic Memory line prints the page tracker's host protections a second (`runtime_memory_protect_count`): ~1000 mprotect calls a second remain in U3's pub with hot pages on, each with TLB shootdowns on every core running the game's threads |
| 0071 | A page is hot after one fault and the set is rebuilt every 60 s (upstream: two faults, 20 s): U3's pub 30.2 -> 35 FPS on top of 0067, faults 520 -> 290/s; the cap process-wide, 8192 pages; `BB_HOT_FAULTS`, `BB_HOT_PERIOD`, `BB_HOT_PAGES_MAX` |
| 0072 | `BB_ADAPTIVE_FLUSH=<low>,<high>`: the mid-frame flush comes as soon as the GPU has at most <low> submissions left and waits (up to twice the interval) while it has at least <high>; U2's palace with 1,3: the same FPS, 15-30% fewer submissions, less GPU idle between them; off without the variable |
| 0073 | The first periodic Memory line prints no host protection rate (its interval is ~0 s) |
| 0074 | A page the GPU writes is never hot: `ChangeRegionState<GPU, true>` drops it from the hot set and protects it again, `CountWriteFaults` skips pages with the GPU bit; hot pages opt-in again (`BB_HOT_PAGES=1`) - a hot page counted as CPU-modified regardless of its GPU bit and its next binding copied guest memory over the GPU's data (the flicker in all three games on 2026-10-23); `BB_CLOBBER_CHECK=1` prints a running total every 100 reports |
| 0075 | The frame rate limit is a live setting (`fps_limit` in bbport.ini, `BB_FPS_LIMIT`, the panel's 30 / 60 / 120 / Off row; the Android panel saves its changes); the velocity-mode scene search compares the game's targets with its display buffer size instead of 1920x1080 (a 720p game was never found: FSR 3.1 / TAA had never run on the G2); `active_render_*` is the scene the upscaler is given; the log says `Upscaler: scene WxH -> output WxH`; the panel shows the game's and the output size, calls FSR 3.1 / TAA anti-aliasing at the game's size and drops the preset and live rows |
| 0076 | The video decoder prints its decode rate every 5 s and its flush and reset calls (`Videodec2: N decodes in 5.0 s ...`): a movie that stops while its audio goes on left no trace in the log |
| 0077 | An on-screen gamepad for phones (`bbport_touchpad.cpp`): a floating left stick, a camera drag, the face buttons, shoulders, triggers, D-pad, L3/R3, Options and the touch pad button drawn by ImGui; `runtime_touch_pad` merges it into every pad read; `touch_pad` 0 / 1 / auto (while no gamepad is connected), opacity, size and camera sensitivity in bbport.ini and the panel; taps held 90 ms; touches no longer reach the game as a mouse (SDL's synthesized left button was R2) |
| 0078 | No audio is queued while the Android app is in the background (`runtime_audio_background` from the window's lifecycle filter) and the first output after it drops what is still queued; a queue 250 ms over its target is dropped too (a device that stopped taking audio) - after the sleep button the sound came 20-30 s late and stayed late: the clock-paced output filled the queue while SDL paused the device, the cadence corrections drain 1/33 of a backlog per second |
| 0079 | `BB_VK_DRIVER_DIR=<dir>`: the custom Vulkan driver is loaded from that directory (the launcher's driver packages: the zips the Android emulators use, from a file or the GitHub releases of K11MCH1, MrPurple and StevenMXZ), the hook libraries stay in the APK's library directory |
| 0080 | FSR 3.1 frame generation no longer leaks a buffer object per job (`gpu/patches/fsr-vulkan/0003`: the bridge's per-frame constant buffers and descriptor pools tagged and retired, as the upscaler does; the Android build applies the fsr-vulkan patches now); the upscaler prints its FSR error code - a crash after minutes with frame generation on (issue #2) |

