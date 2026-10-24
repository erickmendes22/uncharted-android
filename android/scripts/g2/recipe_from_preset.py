#!/usr/bin/env python3
"""recipe_from_preset.py <game> <preset.bin> <image> <out.json> [--check WxH]

A resolution recipe from a BBPATCH2 preset: what the launcher needs to make the preset for any
resolution from the user's own executable, without a byte of the game's code in it.

For every write of the preset the recipe keeps the address, the length and the SHA-256 of the
executable's original bytes there, and then only what changes: integer fields that hold the
resolution (1920 -> W, 1080 -> H, the 240x270 light tile grid -> W/8 x H/4) as formulas, and the
other differing byte runs as they are (new instruction bytes of our own or the community's
patches). The unchanged bytes are read from the user's file when the preset is made
(Launcher.java makePreset). Writes that only matter below 1080p (the resolution fields, the movie
fixes: the chroma plane and the four movie texture lines pinned to 1920x1080) are marked
"below_1080p"; the rest (the low-FPS assert bypass, the logo skip, the job workers' sleep mode,
the navmesh fixes) apply at every resolution.

--check WxH makes the preset for that resolution from the recipe and the image and compares it
with the given preset (the round trip at 1280x720 must be byte for byte).

<game> is u1, u2 or u3; <image> is the executable's image (boot-linked.bin, or a raw image whose
start is the PS4 base 0x400000). The preset and the image are made from the game and stay out of
git; the recipe goes in.
"""
import hashlib
import json
import struct
import sys

BASE = 0x400000
EXECUTABLES = {'u1': 'eboot.bin', 'u2': 'big2-ps4_Shipping.elf', 'u3': 'big3-ps4_Shipping.elf'}
# Original value -> formula; the formula's value at WxH.
FORMULAS = {1920: 'W', 1080: 'H', 240: 'W/8', 270: 'H/4'}
MOVIE_MARKERS = (bytes.fromhex('be00e01f00eb25'),   # our chroma plane fix
                 bytes.fromhex('6a018f'))           # the catalog's movie texture lines


def evaluate(formula, width, height):
    return {'W': width, 'H': height, 'W/8': width // 8, 'H/4': height // 4}[formula]


def load_image(path):
    data = open(path, 'rb').read()
    if data[:8] == b'BBPROBE5':  # boot-linked.bin: the image is the tail
        size = struct.unpack_from('<Q', data, 8)[0]
        data = data[-size:]
    return data


def load_preset(path):
    data = open(path, 'rb').read()
    magic, base, count = struct.unpack_from('<8sQQ', data, 0)
    if magic != b'BBPATCH2' or base != BASE:
        sys.exit(f'{path}: not a BBPATCH2 preset with base {BASE:#x}')
    pos, writes = 24, []
    for _ in range(count):
        offset, length = struct.unpack_from('<QQ', data, pos)
        writes.append((offset, data[pos + 16:pos + 16 + length]))
        pos += 16 + length
    return writes


def differing_runs(old, new):
    runs, j = [], 0
    while j < len(old):
        if old[j] != new[j]:
            k = j
            while k < len(old) and old[k] != new[k]:
                k += 1
            runs.append((j, k))
            j = k
        else:
            j += 1
    return runs


def field_for(old, new, start, end):
    """A 4- or 2-byte little-endian field containing the run whose old value has a formula and
    whose new value is that formula at 1280x720 (the presets were made for it)."""
    for width in (4, 2):
        for begin in range(max(0, end - width), start + 1):
            if begin + width > len(old) or begin > start or end > begin + width:
                continue
            o = int.from_bytes(old[begin:begin + width], 'little')
            n = int.from_bytes(new[begin:begin + width], 'little')
            if o in FORMULAS and evaluate(FORMULAS[o], 1280, 720) == n:
                return {'offset': begin, 'width': width, 'value': FORMULAS[o]}
    return None


def make_recipe(game, writes, image):
    out = []
    for offset, new in writes:
        old = image[offset:offset + len(new)]
        entry = {'address': f'{BASE + offset:#x}', 'length': len(new),
                 'sha256': hashlib.sha256(old).hexdigest()}
        fields, edits = [], []
        for start, end in differing_runs(old, new):
            field = field_for(old, new, start, end)
            if field:
                fields.append(field)
            else:
                edits.append({'offset': start, 'bytes': new[start:end].hex()})
        movie = any(marker in new for marker in MOVIE_MARKERS)
        entry['when'] = 'below_1080p' if fields or movie else 'always'
        if fields:
            entry['fields'] = fields
        if edits:
            entry['edits'] = edits
        out.append(entry)
    return {'game': game, 'executable': EXECUTABLES[game], 'base': f'{BASE:#x}', 'writes': out}


def make_preset(recipe, image, width, height):
    """The preset bytes for WxH from the recipe and the executable's image (the launcher does the
    same in Java); None when a write's original bytes do not match the recipe."""
    entries = []
    for w in recipe['writes']:
        if w['when'] == 'below_1080p' and (width, height) == (1920, 1080):
            continue
        offset = int(w['address'], 16) - BASE
        old = image[offset:offset + w['length']]
        if hashlib.sha256(old).hexdigest() != w['sha256']:
            return None
        new = bytearray(old)
        for e in w.get('edits', []):
            b = bytes.fromhex(e['bytes'])
            new[e['offset']:e['offset'] + len(b)] = b
        for f in w.get('fields', []):
            value = evaluate(f['value'], width, height)
            new[f['offset']:f['offset'] + f['width']] = value.to_bytes(f['width'], 'little')
        entries.append((offset, bytes(new)))
    data = bytearray(struct.pack('<8sQQ', b'BBPATCH2', BASE, len(entries)))
    for offset, new in entries:
        data += struct.pack('<QQ', offset, len(new)) + new
    return bytes(data)


def main():
    args = sys.argv[1:]
    check = None
    if '--check' in args:
        i = args.index('--check')
        check = tuple(int(v) for v in args[i + 1].split('x'))
        del args[i:i + 2]
    game, preset_path, image_path, out_path = args
    image = load_image(image_path)
    writes = load_preset(preset_path)
    recipe = make_recipe(game, writes, image)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(recipe, f, indent=1)
        f.write('\n')
    counts = {}
    for w in recipe['writes']:
        key = w['when'] + ('/fields' if 'fields' in w else '/edits')
        counts[key] = counts.get(key, 0) + 1
    print(f"{out_path}: {len(recipe['writes'])} writes", counts)
    if check:
        made = make_preset(recipe, image, *check)
        same = made == open(preset_path, 'rb').read()
        print(f"round trip at {check[0]}x{check[1]}: {'identical' if same else 'DIFFERENT'}")
        if not same:
            sys.exit(1)


if __name__ == '__main__':
    main()
