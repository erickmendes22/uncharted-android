#!/usr/bin/env python3
"""Check a supplied Bachata XML against catalog hashes and plaintext SELF/ELFs.

No executable is modified. Both XML versions must remain in the catalog. Matching
means byte compatibility, not a claim that a visual effect works on a device.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'scripts'))
from prepare import parse_self

BASE = 0x400000


def load_executable(path):
    source = path.read_bytes()
    if source[:4] == b'\x7fELF':
        elf = source
        header = struct.unpack_from('<16sHHIQQQIHHHHHH', elf)
        if header[0][:7] != b'\x7fELF\x02\x01\x01' or header[2] != 62:
            raise ValueError('expected little-endian x86-64 ELF')
        ph = [dict(zip(('type', 'flags', 'offset', 'vaddr', 'paddr', 'filesz', 'memsz', 'align'),
                       struct.unpack_from('<IIQQQQQQ', elf, header[5] + i * 56)))
              for i in range(header[10])]
    else:
        elf, _, ph, _, _ = parse_self(source)

    def read(address, length):
        address -= BASE
        for p in ph:
            if p['type'] == 1 and p['vaddr'] <= address and address + length <= p['vaddr'] + p['filesz']:
                start = p['offset'] + address - p['vaddr']
                return bytes(elf[start:start + length])
        return b''
    return hashlib.sha256(source).hexdigest(), read


def instructions(data, address):
    with tempfile.NamedTemporaryFile() as f:
        f.write(data)
        f.flush()
        out = subprocess.check_output(['objdump', '-D', '-b', 'binary', '-m', 'i386:x86-64',
                                       '--no-show-raw-insn', '--adjust-vma=' + str(address), f.name], text=True)
    return [m.group(1).strip() for line in out.splitlines()
            if (m := re.match(r'^\s*[0-9a-f]+:\s+(.+)$', line))]


def movie_checks(expected, value, address):
    before, after = instructions(expected, address), instructions(value, address)
    if any('(bad)' in s or s.startswith('.byte') for s in before + after):
        raise ValueError('invalid movie instruction encoding')
    targets = lambda code: [s.split()[1] for s in code if s.startswith('call')]
    if targets(before) != targets(after):
        raise ValueError('movie call destination changed')
    if sum(s.startswith('push') for s in after) != sum(s.startswith('pop') for s in after):
        raise ValueError('unbalanced movie stack rewrite')
    return len(targets(after))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--xml', type=Path, required=True)
    ap.add_argument('--catalog', type=Path, required=True)
    ap.add_argument('--exe', action='append', default=[], help='canonical executable name=local path')
    ap.add_argument('--report', type=Path, required=True)
    args = ap.parse_args()
    catalog = json.loads(args.catalog.read_text())['patches']
    index = collections.defaultdict(list)
    for p in catalog:
        index[p['name'], p['elf'], p['version']].append(p)
    patches = []
    movie_calls = 0
    versions = collections.Counter()
    for m in ET.parse(args.xml).getroot().findall('Metadata'):
        key = m.get('Name'), m.get('AppElf'), m.get('AppVer')
        writes, spans = [], []
        for line in m.findall('./PatchList/Line'):
            if line.get('Type') != 'bytes':
                raise ValueError('unsupported patch type')
            expected, value = bytes.fromhex(line.get('Expected')), bytes.fromhex(line.get('Value'))
            address = int(line.get('Address'), 16)
            if not expected or len(expected) != len(value) or address < BASE:
                raise ValueError('invalid patch length/address')
            if any(address < end and start < address + len(value) for start, end in spans):
                raise ValueError('overlapping writes within a patch')
            spans.append((address, address + len(value)))
            writes.append(dict(address=line.get('Address'), length=len(value),
                               sha256=hashlib.sha256(expected).hexdigest(), bytes=value.hex()))
            # Long resolution writes replace the movie texture allocation calls.
            if key[0].startswith('Resolution:') and len(expected) > 32:
                movie_calls += movie_checks(expected, value, address)
        matches = index[key]
        if len(matches) != 1 or matches[0]['writes'] != writes:
            raise ValueError('catalog differs from supplied XML: ' + repr(key))
        patches.append((key, m, writes))
        versions[key[2]] += 1
    if set(versions) != {'01.00', '01.02'}:
        raise ValueError('both 01.00 and 01.02 are required')
    results = []
    for item in args.exe:
        name, filename = item.split('=', 1)
        digest, read = load_executable(Path(filename))
        groups = collections.defaultdict(list)
        matches = collections.Counter()
        for key, metadata, writes in patches:
            if key[1] != name:
                continue
            fits = all(read(int(line.get('Address'), 16), len(bytes.fromhex(line.get('Expected')))) == bytes.fromhex(line.get('Expected'))
                       for line in metadata.findall('./PatchList/Line'))
            if fits:
                groups[key[0]].append(key[2])
                matches[key[2]] += 1
        names = {key[0] for key, _, _ in patches if key[1] == name}
        if set(groups) != names or any(len(v) != 1 for v in groups.values()):
            raise ValueError('missing or ambiguous runtime patch match: ' + name)
        results.append(dict(executable=name, sha256=digest, matching_versions=dict(matches),
                            resolution_matches={k: v[0] for k, v in groups.items() if k.startswith('Resolution:')}))
    report = dict(xml_sha256=hashlib.sha256(args.xml.read_bytes()).hexdigest(),
                  catalog_matches_xml=True, patches_by_version=dict(versions),
                  movie_call_targets_preserved=movie_calls, intra_patch_overlaps=0,
                  executables=results,
                  limits='01.00 binaries checked directly. 01.02 catalog preserved exactly from XML; no 01.02 executable supplied. No APK build or device test.')
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
