#!/usr/bin/env python3
"""Retag the official Bachata S4 patches for Uncharted (01.02 executables) to AppVer 01.00.

The CUSA02344 dump has APP_VER=01.00, VERSION=01.02 in param.sfo, while eboot/big2/big3 are build 01.02.
Bachata picks patches by APP_VER and offers the 01.00 variants, whose Expected bytes are not found in
these files: the patches are silently not applied (resolution, the low-FPS assert bypass).
The script takes the AppVer="01.02" variants from the catalog XML (assets/patches/... inside the APK) and changes
only AppVer and the name; addresses/bytes stay. Check: --check <directory with *.image.bin> compares Expected
(patch addresses = image offset + 0x400000).

  python3 bachata_retag_patches.py <PATCHES-dir> <out.xml> [--check ~/src/inventory/code]
"""
import os
import re
import sys

SOURCES = ['UnchartedTheNathanDrakeCollection.xml',
           'Uncharted_NathanDrakeCollection_Plus_Optional.xml',
           'Uncharted_NathanDrakeCollection_Resolution.xml']
TITLE_IDS = ['CUSA02344', 'CUSA02320', 'CUSA02343', 'CUSA01399', 'CUSA02826']
IMAGES = {'eboot.bin': 'eboot', 'big2-ps4_Shipping.elf': 'big2', 'big3-ps4_Shipping.elf': 'big3'}
PATCH_BASE = 0x400000


def retag(src_dir):
    out, seen = [], set()
    for fn in SOURCES:
        s = open(os.path.join(src_dir, fn), encoding='utf-8').read()
        for m in re.finditer(r'<Metadata\b[^>]*>.*?</Metadata>', s, re.S):
            blk = m.group(0)
            head = blk[:blk.index('>')]
            if 'AppVer="01.02"' not in head or 'macOS' in head:
                continue
            name = re.search(r'Name="([^"]*)"', head).group(1)
            elf = re.search(r'AppElf="([^"]*)"', head).group(1)
            key = (name.replace('[UNVERIFIED] ', ''), elf)
            if key in seen:
                continue
            seen.add(key)
            blk = blk.replace('AppVer="01.02"', 'AppVer="01.00"', 1)
            blk = blk.replace('Name="%s"' % name, 'Name="[exe 01.02] %s"' % name, 1)
            blk = blk.replace('isEnabled="true"', 'isEnabled="false"')
            out.append('    ' + blk)
    ids = ''.join('        <ID>%s</ID>\n' % i for i in TITLE_IDS)
    return ('<?xml version="1.0"?>\n<!-- Bachata S4 Uncharted patches for the 01.02 executables, '
            're-tagged AppVer 01.00 (param.sfo APP_VER 01.00). Bytes unchanged. -->\n<Patch>\n'
            '    <TitleID>\n' + ids + '    </TitleID>\n' + '\n'.join(out) + '\n</Patch>\n'), len(out)


def check(xml, img_dir):
    bad = 0
    for m in re.finditer(r'<Metadata\b([^>]*)>(.*?)</Metadata>', xml, re.S):
        head, body = m.groups()
        name = re.search(r'Name="([^"]*)"', head).group(1)
        elf = re.search(r'AppElf="([^"]*)"', head).group(1)
        d = open(os.path.join(img_dir, IMAGES[elf] + '.image.bin'), 'rb').read()
        for a, e in re.findall(r'Address="(0x[0-9a-f]+)" Expected="([0-9a-f]+)"', body):
            o = int(a, 16) - PATCH_BASE
            if d[o:o + len(e) // 2].hex() != e:
                print('mismatch:', name, a)
                bad += 1
                break
    return bad


if __name__ == '__main__':
    xml, n = retag(sys.argv[1])
    open(sys.argv[2], 'w', encoding='utf-8', newline='\n').write(xml)
    print(n, 'patches ->', sys.argv[2])
    if '--check' in sys.argv:
        bad = check(xml, sys.argv[sys.argv.index('--check') + 1])
        print('all Expected bytes match' if not bad else '%d patches do not match' % bad)
