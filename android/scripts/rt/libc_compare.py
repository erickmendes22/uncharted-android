#!/usr/bin/env python3
"""Comparing libc_diff output: x86 native vs lifted aarch64. Per function:
OK (all lines match), DIFF (the first difference), SKIP (did not complete natively)."""
import re
import sys


def parse(path):
    funcs = {}
    for line in open(path, errors='replace'):
        line = line.rstrip('\n')
        m = re.match(r'^([a-z0-9_]+)(?:[(:= ]|$)', line)
        if not m:
            continue
        funcs.setdefault(m.group(1), []).append(line)
    return funcs


def status(lines):
    for l in lines:
        if 'CRASH' in l or 'exit ' in l or 'not exported' in l:
            return l
    return None


x86, arm = parse(sys.argv[1]), parse(sys.argv[2])
ok = diff = skip = 0
for name in x86:
    xs, as_ = x86[name], arm.get(name, [])
    bad = status(xs)
    if bad:
        skip += 1
        print(f'SKIP {name:10s} native: {bad.split(":", 1)[1].strip()}')
        continue
    if xs == as_:
        ok += 1
        print(f'OK   {name:10s} {len(xs)} results')
        continue
    diff += 1
    first = next((i for i, (a, b) in enumerate(zip(xs, as_)) if a != b), min(len(xs), len(as_)))
    a = as_[first] if first < len(as_) else '<missing>'
    esc = next((l for l in as_ if 'ESCAPE' in l or 'CRASH' in l or 'exit ' in l), '')
    print(f'DIFF {name:10s} line {first}: x86 {xs[first][:70]!r} arm {a[:70]!r} {esc}')
print(f'\n{ok} OK, {diff} DIFF, {skip} skipped (crash natively without imports/TLS)')
