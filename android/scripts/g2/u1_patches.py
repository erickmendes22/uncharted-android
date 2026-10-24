#!/usr/bin/env python3
"""Build U1 (eboot.bin 01.02) patches from Bachata S4 catalog XML into a patches.bin for the port's
loader. U2/U3: `--elf big2-ps4_Shipping.elf` / `big3-ps4_Shipping.elf`, Expected bytes checked with
`--check-elf <out>/eboot.elf` (the port's prepare.py output for that executable).

Bachata's patches (resolution, shadows, effects) change bytes of the game's code, not emulator
functions, so they suit the port as well: the loader (`--patches`, the BBPATCH2 format of
uncharted_pc/scripts/patches.py) writes them into the image after relocation; addresses are PS4
virtual addresses (eboot base 0x400000), image offset = address - 0x400000. The loader does not
check Expected bytes - this script does (--check <eboot image> or --check-elf). Our own XML holds
ExpectedSha256 instead: the SHA-256 of the original bytes (as many as Value has), no game bytes.

Source: catalog XML from the Bachata APK (assets/patches/.../PATCHES/*.xml), or retagged by
bachata_retag_patches.py, plus our own XML (u1_extra_patches.xml, u23_extra_patches.xml): entries
with the chosen AppElf and AppVer="01.02", or named "[exe 01.02] ...". Patch names drop the
"[exe 01.02] " and "[UNVERIFIED] " prefixes.

  python3 u1_patches.py <out.bin> <xml>... --select "Resolution: 720p (performance)" \
      [--select ...] [--lines "<name>=<address>,<address>"] [--check ~/src/inventory/code/eboot.image.bin]
  python3 u1_patches.py - <xml>... --list
  python3 u1_patches.py <out.bin> u23_extra_patches.xml --elf big2-ps4_Shipping.elf \
      --base u2_720f.bin --select "Uncharted 2: Job workers sleep when idle" --check-elf <big2 eboot.elf>

--base starts from the writes of an existing patches.bin (a preset), so a preset can be extended
without its original recipe; writes to the same bytes are still refused.

Movies at a lower resolution: the player (movie-rendering, @0x8fb7b1) takes the movie frame size
from the display configuration's output size, while the movies are 1080p. The S4+ lines of
"Uncharted 1: Resolution: ..." @0x8fb7b1 and @0x9de4fe/53d/597/5db pin it and the movie textures
to 1920x1080 (the same in every S4+ resolution) - take them with --lines together with the full
resolution patch (all 5 size pairs).
"""
import argparse
import hashlib
import struct
import sys
import xml.etree.ElementTree as ET

EBOOT_BASE = 0x400000
PREFIXES = ('[exe 01.02] ', '[UNVERIFIED] ')


def short_name(name):
    changed = True
    while changed:
        changed = False
        for p in PREFIXES:
            if name.startswith(p):
                name, changed = name[len(p):], True
    return name


def encode(line):
    kind, value = line.get('Type'), line.get('Value')
    if kind == 'bytes':
        return bytes.fromhex(value.replace(' ', ''))
    if kind in ('bytes16', 'bytes32', 'bytes64'):
        return int(value, 0).to_bytes(int(kind[5:]) // 8, 'little')
    if kind == 'float32':
        return struct.pack('<f', float(value))
    if kind == 'float64':
        return struct.pack('<d', float(value))
    raise ValueError('unsupported patch type %r' % kind)


def elf_image(path):
    """The PT_LOAD/PT_SCE_RELRO segments of an ELF at their p_vaddr (vaddr 0 at offset 0), e.g.
    eboot.elf of the port's prepare.py output for big2/big3."""
    data = open(path, 'rb').read()
    if data[:4] != b'\x7fELF':
        sys.exit('%s: not an ELF' % path)
    phoff, = struct.unpack_from('<Q', data, 0x20)
    phentsize, phnum = struct.unpack_from('<HH', data, 0x36)
    image = bytearray()
    for i in range(phnum):
        p_type, _, p_offset, p_vaddr, _, p_filesz = struct.unpack_from('<IIQQQQ', data, phoff + i * phentsize)
        if p_type not in (1, 0x61000010):
            continue
        if len(image) < p_vaddr + p_filesz:
            image.extend(bytes(p_vaddr + p_filesz - len(image)))
        image[p_vaddr:p_vaddr + p_filesz] = data[p_offset:p_offset + p_filesz]
    return bytes(image)


def matches(have, expected):
    """Expected bytes (the catalog), or the hex SHA-256 of them (our own XML)."""
    if isinstance(expected, str):
        return hashlib.sha256(have).hexdigest() == expected
    return have == expected


def eboot_patches(files, elf='eboot.bin'):
    """{short name: [(offset, expected bytes or their hex SHA-256 or None, data)]} of the 01.02
    patches of one executable of the collection (eboot.bin = U1, big2-ps4_Shipping.elf,
    big3-ps4_Shipping.elf)."""
    found = {}
    for path in files:
        for meta in ET.parse(path).getroot().iter('Metadata'):
            name = meta.get('Name', '')
            if meta.get('AppElf', 'eboot.bin') != elf:
                continue
            if meta.get('AppVer') != '01.02' and not name.startswith('[exe 01.02]'):
                continue
            writes = []
            for line in meta.iter('Line'):
                expected, digest = line.get('Expected'), line.get('ExpectedSha256')
                data = encode(line)
                if expected and len(bytes.fromhex(expected)) != len(data):
                    raise ValueError('%s @%s: Value and Expected differ in length'
                                     % (name, line.get('Address')))
                check = bytes.fromhex(expected) if expected else digest.lower() if digest else None
                writes.append((int(line.get('Address'), 0) - EBOOT_BASE, check, data))
            key = short_name(name)
            if key in found and found[key] != writes:
                raise ValueError('%s: different variants in the catalog' % key)
            found[key] = writes
    return found


def read_patches_bin(path):
    """The writes [(offset, data)] of a BBPATCH2 file."""
    blob = open(path, 'rb').read()
    magic, base, count = struct.unpack_from('<8sQQ', blob, 0)
    if magic != b'BBPATCH2' or base != EBOOT_BASE:
        sys.exit('%s: not a BBPATCH2 file with base %#x' % (path, EBOOT_BASE))
    writes, pos = [], 24
    for _ in range(count):
        offset, size = struct.unpack_from('<QQ', blob, pos)
        pos += 16
        writes.append((offset, blob[pos:pos + size]))
        pos += size
    return writes


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('out')
    p.add_argument('xml', nargs='+')
    p.add_argument('--select', action='append', default=[], help='patch name (repeatable)')
    p.add_argument('--lines', action='append', default=[],
                   help='"<patch name>=<address>,...": only these lines of a patch (repeatable)')
    p.add_argument('--base', help='start from the writes of this patches.bin (BBPATCH2)')
    p.add_argument('--check', help='eboot image (vaddr 0 at offset 0): Expected bytes must match')
    p.add_argument('--check-elf', help='the same check against an ELF (prepare.py eboot.elf)')
    p.add_argument('--elf', default='eboot.bin',
                   help='executable: eboot.bin (U1), big2-ps4_Shipping.elf, big3-ps4_Shipping.elf')
    p.add_argument('--list', action='store_true', help='list the 01.02 patches and exit')
    a = p.parse_args()
    patches = eboot_patches(a.xml, a.elf)
    if a.list:
        for name in sorted(patches):
            print(name)
        return 0
    chosen = [(n, patches.get(n)) for n in a.select]
    for spec in a.lines:
        name, _, addrs = spec.rpartition('=')
        wanted = {int(x, 0) - EBOOT_BASE for x in addrs.split(',')}
        lines = [w for w in patches.get(name, []) if w[0] in wanted]
        if len(lines) != len(wanted):
            sys.exit('%s: not all of %s found' % (name, addrs))
        chosen.append((name, lines))
    missing = [n for n, w in chosen if w is None]
    if missing:
        sys.exit('not in the catalog: %s' % missing)
    image = open(a.check, 'rb').read() if a.check else elf_image(a.check_elf) if a.check_elf else None
    writes, owner = [], {}
    if a.base:
        base = 'base %s' % a.base
        for offset, data in read_patches_bin(a.base):
            for b in range(offset, offset + len(data)):
                owner[b] = base
            writes.append((offset, data))
    for name, lines in chosen:
        for offset, expected, data in lines:
            for b in range(offset, offset + len(data)):
                if b in owner and owner[b] != name:
                    sys.exit('%s and %s write the same bytes (%#x)' % (owner[b], name, b + EBOOT_BASE))
                owner[b] = name
            if image is not None and expected is not None and \
                    not matches(image[offset:offset + len(data)], expected):
                sys.exit('%s: Expected bytes differ at %#x - another build of eboot?'
                         % (name, offset + EBOOT_BASE))
            writes.append((offset, data))
    blob = struct.pack('<8sQQ', b'BBPATCH2', EBOOT_BASE, len(writes))
    for offset, data in writes:
        blob += struct.pack('<QQ', offset, len(data)) + data
    open(a.out, 'wb').write(blob)
    print('%d writes from %d patches%s -> %s%s' % (len(writes), len(chosen), ' + base' if a.base else '',
                                                   a.out, '' if image is not None else ' (Expected NOT checked)'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
