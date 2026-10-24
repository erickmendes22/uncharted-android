# GPU faults on the G2: KGSL snapshots and run series

Tools for analysing CP faults (device loss) without root. Snapshots, logs and dmesg stay local only
(a snapshot holds the game's GPU data), not in git.

## Series

- `dlbatch.sh <dir> <n> [KEY=VALUE ...]`: n sessions through `../runs/session.sh` (the owner's env +
  variables). After each: the kernel's GPU lines since the session started (`r<i>-dmesg.txt`), the
  KGSL snapshot (`r<i>-snap.bin`; reading frees the slot - KGSL keeps the first unread one), a summary:
  faults (`opcode error`, `Protected mode`, `page fault`), AHB, IFPC, preemptions, FPS.
  `NO_IFPC=1`: the GPU does not sleep during the session (`kgslpwr 0` before, `kgslpwr 1` after and on exit).
- `snapgrab.sh <dir>`: in the background, every 2 s, if KGSL's fault counter grew, reads the snapshot
  (and frees the slot for the next one): snapshots of all faults of a series, not only the first.
- `thermlog.sh <file> [secs]`: every 15 s while the game runs: the hottest `cpu-1-*` (gold),
  `cpu-2-*` (prime), `gpuss-*` zones (deg C) and the clocks of cpu3-7 (no root).
- Checking recovery after a lost context (mesa-0013): `TU_KGSL_FAULT_TEST=200i` among the session's
  variables: 200 s after the first submit, zeros before the IBs of four of the game's submits;
  the game log shows `tu/kgsl: ... the queue goes on in new context ...`, dmesg `gpu fault threshold exceeded`.
- `kgslpwr.c`: `IOCTL_KGSL_SETPROPERTY(KGSL_PROP_PWRCTRL, 0|1)`: 0 keeps the GPU on (no IFPC);
  **the flag is global and outlives the process**: always `kgslpwr 1` after an experiment. Build and
  install: `aarch64-linux-android33-clang -O2 -o kgslpwr kgslpwr.c`, `adb push kgslpwr /data/local/tmp/`.

## Snapshot (`/sys/class/kgsl/kgsl-3d0/snapshot/dump`)

- `snapparse.py <snap> [--find ...] [--dump ...]`: sections, IBs, rings; `objlist.py`: all GPU objects.
- `snapregs2.py`, `bvregs.py`: CP registers (BR and BV: IB1/IB2 and remainders, `CP_PROTECT_STATUS`);
  `protect48.py <snap> <reg>`: the `CP_PROTECT` table (48 entries).
- `ib1ctl.py <snap> <ib1> [rem]`: IB1 control packets (IB2 calls, THREAD_CONTROL, markers) and the
  CP fetch position; `crbdump.py <snap> <ib>`: all packets of an IB, `CP_CONTEXT_REG_BUNCH` in pairs.
- `sdstable.py <snap> <ib1> <ib2> <ib2 rem>`: the table of `CP_SET_DRAW_STATE` groups at the fault
  position and a check of their contents (if the memory is in the snapshot); `sdscross.py`, `sdspkt.py`,
  `sdsstraddle.py`: per draw: groups across a page boundary, the packet's entries and where the fetch
  stopped, packets across 512 B...4 KB boundaries.
- `rbibs.py <snap> [rb]`: IB1 calls in the KGSL ring (a buffer queued again).

The fault position from the kernel log: `status ... ib1 <address>/<remainder> ib2 <address>/<remainder>`;
the CP stands `size - remainder` dwords from the start of the IB (with prefetch). `faultpos.py <snapshot>...`
does that itself: the BR and BV fetch positions in IB1/IB2 and the packet they are inside (and whether
it crosses 1/2 KB boundaries).

## GPU profile

- `gprof_idle.py <log> [blocks] [labels]`: GPU idle between submits and the top labels from
  `BB_GPU_PROFILE=1` (ms/s ~ share of time x1000).
