#!/usr/bin/env python3
"""The public app's patch presets as APK assets: <assets>/patches/<preset> and <assets>/presets.txt.

  preset_assets.py <assets dir> <presets dir> u1=<boot-linked.bin> u2=<...> u3=<...>

presets.txt has a line "<preset> <game> <sha256>" per preset: the SHA-256 of the executable's
bytes at every write of the preset, in the preset's order, taken from the boot-linked.bin the
preset was made for (the image is the file's tail; its size is the header's second field). The
launcher (Launcher.java presetFits) hashes the same bytes of the user's prepared executable and
applies the preset only on a match: the presets are byte writes for one version of the game. The
presets and the boot images are made from the game; neither goes into git.
"""
import hashlib
import shutil
import struct
import sys
from pathlib import Path

PRESETS = {'u1': 'o720n.bin', 'u2': 'u2_720fs2dk.bin', 'u3': 'u3_720f.bin'}


def writes(preset):
    data = preset.read_bytes()
    magic, base, count = struct.unpack_from('<8sQQ', data, 0)
    if magic != b'BBPATCH2' or base != 0x400000:
        sys.exit(f'{preset}: not a BBPATCH2 preset with base 0x400000')
    pos, out = 24, []
    for _ in range(count):
        offset, length = struct.unpack_from('<QQ', data, pos)
        out.append((offset, length))
        pos += 16 + length
    return out


def original_hash(boot, preset):
    data = boot.read_bytes()
    if data[:8] != b'BBPROBE5':
        sys.exit(f'{boot}: not a BBPROBE5 boot image')
    image_size, = struct.unpack_from('<Q', data, 8)
    image = memoryview(data)[len(data) - image_size:]
    sha = hashlib.sha256()
    for offset, length in writes(preset):
        if offset + length > image_size:
            sys.exit(f'{preset}: a write past the image')
        sha.update(image[offset:offset + length])
    return sha.hexdigest()


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    assets, presets = Path(sys.argv[1]), Path(sys.argv[2])
    boots = dict(arg.split('=', 1) for arg in sys.argv[3:])
    (assets / 'patches').mkdir(parents=True, exist_ok=True)
    lines = []
    for game, preset in PRESETS.items():
        shutil.copyfile(presets / preset, assets / 'patches' / preset)
        lines.append(f'{preset} {game} {original_hash(Path(boots[game]), presets / preset)}')
    (assets / 'presets.txt').write_text('\n'.join(lines) + '\n')
    print('\n'.join(lines))


if __name__ == '__main__':
    main()
