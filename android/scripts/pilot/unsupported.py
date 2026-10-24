#!/usr/bin/env python3
"""Which instructions of the guest image Remill does not lift (sync_hyper_call kAMD64EmulateInstruction
or a decoding error). Every unique (mnemonic, bytes) is lifted separately.
Run in WSL: python3 -I unsupported.py guest.x86.elf [remill-lift]"""
import os
import re
import subprocess
import sys

elf = sys.argv[1]
lift = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/src/remill/install/bin/remill-lift-22')
dis = subprocess.run(['llvm-objdump-22', '-d', '-M', 'intel', elf], capture_output=True, text=True,
                     check=True).stdout
seen = {}
func = '?'
for line in dis.splitlines():
    m = re.match(r'^[0-9a-f]+ <(\w+)>:', line)
    if m:
        func = m.group(1)
        continue
    m = re.match(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$', line)
    if not m:
        continue
    addr, raw, text = m.groups()
    mnem = text.split()[0] if text else '?'
    if mnem in ('lock', 'rep', 'repne'):
        mnem = ' '.join(text.split()[:2])
    key = (mnem, raw.strip())
    seen.setdefault(key, (func, addr, text.strip()))

bad = {}
for (mnem, raw), (func, addr, text) in seen.items():
    hexbytes = raw.replace(' ', '')
    r = subprocess.run([lift, '--arch', 'amd64_avx', '--os', 'linux', '--address', '0x' + addr,
                        '--bytes', hexbytes, '--ir_out', '/dev/stdout'], capture_output=True, text=True)
    ir = r.stdout
    if r.returncode or '__remill_sync_hyper_call' in ir and 'i32 257' in ir or '__remill_error' in ir:
        why = 'lift failed' if r.returncode else ('emulate(0x101)' if 'i32 257' in ir else 'error')
        bad.setdefault(mnem, []).append(f'{func}+{addr}: {text}  [{why}]')
print(f'{len(seen)} unique instruction encodings, {sum(map(len, bad.values()))} not lifted')
for mnem, items in sorted(bad.items()):
    print(f'{mnem}:')
    for it in items[:4]:
        print('   ', it)
