#!/usr/bin/env python3
# plt_names.py <eboot.elf> <import_names.inc> [name ...]: the PLT stubs of a PS4 ELF (as prepared by
# uncharted_pc's prepare.py) with their import names; with names, only those. Addresses as the port
# loads the image (+0x400000).
import re
import struct
import sys

BASE = 0x400000
elf = open(sys.argv[1], 'rb').read()
names = {k.split('#')[0]: v for k, v in re.findall(r'\{"([^"]+)","([^"]+)"\}', open(sys.argv[2]).read())}
want = set(sys.argv[3:])

phoff, = struct.unpack_from('<Q', elf, 0x20)
phnum, = struct.unpack_from('<H', elf, 0x38)
ph = []
for i in range(phnum):
    t, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack_from('<IIQQQQQQ', elf, phoff + 56 * i)
    ph.append(dict(type=t, offset=off, vaddr=vaddr, filesz=filesz, memsz=memsz))
dp = next(p for p in ph if p['type'] == 2)
tags = {}
for pos in range(dp['offset'], dp['offset'] + dp['filesz'], 16):
    tag, value = struct.unpack_from('<QQ', elf, pos)
    if tag == 0:
        break
    tags.setdefault(tag, value)
lib = next(p for p in ph if p['type'] == 0x61000000)
blob = elf[lib['offset']:lib['offset'] + lib['filesz']]
strings = blob[tags[0x61000035]:tags[0x61000035] + tags[0x61000037]]
def string(off):
    return strings[off:strings.index(0, off)].decode('ascii')
syms = blob[tags[0x61000039]:tags[0x61000039] + tags[0x6100003f]]
symbols = [string(struct.unpack_from('<I', syms, pos)[0]) for pos in range(0, len(syms), 24)]
slot_name = {}
table = blob[tags[0x61000029]:tags[0x61000029] + tags[0x6100002d]]  # JMPREL (jump slots)
for pos in range(0, len(table), 24):
    target, info, addend = struct.unpack_from('<QQq', table, pos)
    nid = symbols[info >> 32]
    slot_name[target] = names.get(nid.split('#')[0], nid)

code = next(p for p in ph if p['type'] == 1)
text = elf[code['offset']:code['offset'] + code['filesz']]
for m in re.finditer(rb'\xff\x25', text):
    at = m.start()
    disp, = struct.unpack_from('<i', text, at + 2)
    slot = code['vaddr'] + at + 6 + disp
    if slot in slot_name and (not want or slot_name[slot] in want):
        print(f'{BASE + code["vaddr"] + at:#x} {slot_name[slot]}')
