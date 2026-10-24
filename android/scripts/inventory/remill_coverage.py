#!/usr/bin/env python3
"""Inventory, step 3: coverage by Remill forms. Every unique iform (over all modules)
is lifted separately from its sample; then the share of functions that lift completely.
Run in WSL: python3 -I remill_coverage.py ~/src/inventory [remill-lift]
Writes <dir>/coverage.tsv and <dir>/report.md (no game bytes: only statistics by form)."""
import collections
import concurrent.futures as cf
import os
import re
import subprocess
import sys
from pathlib import Path

W = Path(sys.argv[1])
LIFT = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/src/remill/install/bin/remill-lift-22')
MODULES = ['eboot', 'big2', 'big3', 'libc', 'libSceFios2']

forms = {}           # iform -> dict
per_mod = collections.defaultdict(dict)
for mod in MODULES:
    tsv = W / 'xed' / f'{mod}.iforms.tsv'
    if not tsv.exists():
        continue
    for line in tsv.read_text().splitlines():
        iform, count, funcs, isa, ext, addr, hexb, disasm, fid = line.split('\t')
        per_mod[mod][iform] = (int(count), int(funcs))
        f = forms.setdefault(iform, dict(count=0, funcs=0, isa=isa, ext=ext, addr=addr, hex=hexb,
                                         disasm=disasm, id=int(fid)))
        f['count'] += int(count)
        f['funcs'] += int(funcs)


def lift(item):
    iform, f = item
    r = subprocess.run([LIFT, '--arch', 'amd64_avx', '--os', 'linux', '--address', '0x' + f['addr'],
                        '--bytes', f['hex'], '--ir_out', '/dev/stdout'], capture_output=True, text=True)
    ir = r.stdout
    calls = set(re.findall(r'call [^@]*@(__remill_[a-z_0-9]+)\(', ir))
    # 257 = kAMD64EmulateInstruction: the form has no semantics. __remill_error itself is legitimate
    # (DIV -> #DE, UD2, HLT), hence a separate status.
    hyper = re.findall(r'call [^@]*@__remill_sync_hyper_call\(.*?i32 (?:noundef )?(\d+)\)', ir)
    if r.returncode or 'define' not in ir:
        status = 'lift_failed'
    elif '257' in hyper:
        status = 'no_semantics'
    elif '__remill_error' in calls:
        status = 'error_path'
    elif hyper:
        status = 'hypercall_' + ','.join(sorted(set(hyper)))
    else:
        status = 'ok'
    return iform, status


with cf.ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
    status = dict(ex.map(lift, forms.items()))

bad = {k for k, s in status.items() if s in ('no_semantics', 'lift_failed')}
with open(W / 'coverage.tsv', 'w') as out:
    for iform, f in sorted(forms.items(), key=lambda kv: -kv[1]['funcs']):
        out.write(f"{iform}\t{status[iform]}\t{f['count']}\t{f['funcs']}\t{f['isa']}\t{f['ext']}\t{f['disasm']}\n")

id_bad = {forms[k]['id'] for k in bad}
rep = ['# Guest code inventory x Remill', '',
       f'Unique forms (XED iform): {len(forms)}; without semantics in Remill: {len(bad)}.', '',
       '| module | functions | fully liftable | % functions | % function code bytes | instructions | % instructions without semantics |',
       '|---|---|---|---|---|---|---|']
for mod in MODULES:
    fn = W / 'xed' / f'{mod}.funcs'
    if not fn.exists():
        continue
    n = okn = 0
    nbytes = okbytes = 0
    for line in fn.read_text().splitlines():
        parts = line.split()
        size = int(parts[1], 16)
        ids = set(map(int, parts[2:]))
        n += 1; nbytes += size
        if not ids & id_bad:
            okn += 1; okbytes += size
    tot = sum(c for c, _ in per_mod[mod].values())
    badc = sum(c for k, (c, _) in per_mod[mod].items() if k in bad)
    rep.append(f'| {mod} | {n} | {okn} | {okn / n * 100:.1f} | {okbytes / max(nbytes, 1) * 100:.1f} | '
               f'{tot} | {badc / max(tot, 1) * 100:.2f} |')

rep += ['', '## Forms without semantics, by the number of affected functions (all modules)', '',
        '| iform | ISA set | functions | instructions | status |', '|---|---|---|---|---|']
for iform, f in sorted(((k, forms[k]) for k in bad), key=lambda kv: -kv[1]['funcs']):
    rep.append(f"| {iform} | {f['isa']} | {f['funcs']} | {f['count']} | {status[iform]} |")

by_isa = collections.Counter()
for k in bad:
    by_isa[forms[k]['isa']] += forms[k]['funcs']
rep += ['', '## Without semantics by ISA set (sum of affected functions)', '']
rep += [f'- {isa}: {n}' for isa, n in by_isa.most_common()]

other = collections.Counter(s for s in status.values() if s not in ('ok', 'no_semantics', 'lift_failed'))
rep += ['', '## Other statuses', '']
for s, n in other.most_common():
    rep.append(f'- {s}: {n} forms: ' + ', '.join(sorted(k for k, v in status.items() if v == s))[:400])

isa_all = collections.Counter()
for k, f in forms.items():
    isa_all[f['isa']] += f['count']
rep += ['', '## All instructions by ISA set (number of instructions)', '']
rep += [f'- {isa}: {n}' for isa, n in isa_all.most_common(40)]

(W / 'report.md').write_text('\n'.join(rep) + '\n')
print('\n'.join(rep[:12 + len(MODULES)]))
print(f'... full report: {W / "report.md"}')
