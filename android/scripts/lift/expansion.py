#!/usr/bin/env python3
"""Per-function expansion: the size of sub_<addr> in the ARM object against the function's x86 size (FDE).
Run in WSL: python3 -I expansion.py <obj.o> <mod.ranges> <load_base_hex> [top]"""
import subprocess
import sys

obj, ranges, base = sys.argv[1], sys.argv[2], int(sys.argv[3], 16)
top = int(sys.argv[4]) if len(sys.argv) > 4 else 15
x86 = {}
for line in open(ranges):
    a, s = line.split()
    x86[base + int(a, 16)] = int(s, 16)
nm = subprocess.run(['llvm-nm-22', '--size-sort', '-S', '--defined-only', obj], capture_output=True,
                    text=True, check=True).stdout
arm = {}
internal = 0
for line in nm.splitlines():
    parts = line.split()
    if len(parts) != 4 or not parts[3].startswith('sub_'):
        continue
    addr = int(parts[3][4:], 16)
    size = int(parts[1], 16)
    if addr in x86:
        arm[addr] = arm.get(addr, 0) + size
    else:
        internal += size  # traces of switch targets / other heads
tot_x86 = sum(x86[a] for a in arm)
tot_arm = sum(arm.values())
print(f'functions {len(arm)}: x86 {tot_x86 // 1024} KiB -> ARM {tot_arm // 1024} KiB ({tot_arm / max(tot_x86, 1):.1f}x); '
      f'internal traces (switch targets etc.) {internal // 1024} KiB')
rows = sorted(arm, key=lambda a: -arm[a])[:top]
print(f'{"addr":>12} {"x86":>7} {"arm":>8} {"x":>6}')
for a in rows:
    print(f'{a:12x} {x86[a]:7d} {arm[a]:8d} {arm[a] / max(x86[a], 1):6.1f}')
# distribution of the ratios
ratios = sorted(arm[a] / max(x86[a], 1) for a in arm)
print('ratio percentiles: ' + ' '.join(f'p{p}={ratios[len(ratios) * p // 100]:.1f}' for p in (10, 50, 90, 99)))
