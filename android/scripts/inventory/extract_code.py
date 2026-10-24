#!/usr/bin/env python3
"""Guest code inventory, step 1: executable segments + function boundaries (from .eh_frame).
Uses parse_self from uncharted_pc/scripts/prepare.py (the dump is already decrypted; this is only
unpacking a fake-signed SELF into an ELF, as the port itself does).
Run in WSL: python3 -I extract_code.py <game folder> <output>
Output (outside git!): <name>.seg<i>.bin, <name>.image.bin, <name>.json (segments, functions, summary)."""
import json
import struct
import sys
from pathlib import Path

PORT_SCRIPTS = Path(__file__).resolve().parents[3] / 'scripts'
sys.path.insert(0, str(PORT_SCRIPTS))
from prepare import parse_self  # noqa: E402

EXES = ['eboot.bin', 'big2-ps4_Shipping.elf', 'big3-ps4_Shipping.elf',
        'sce_module/libc.prx', 'sce_module/libSceFios2.prx']
PT_LOAD, PT_GNU_EH_FRAME, PT_SCE_RELRO = 1, 0x6474e550, 0x61000010


def read_encoded(buf, pos, enc, base_vaddr):
    """DW_EH_PE: returns (value, new position). base_vaddr is the address of buf[0]."""
    if enc == 0xff:
        return None, pos
    fmt = enc & 0x0f
    app = enc & 0x70
    if fmt == 0x00:   v, n = struct.unpack_from('<Q', buf, pos)[0], 8
    elif fmt == 0x03: v, n = struct.unpack_from('<I', buf, pos)[0], 4
    elif fmt == 0x04: v, n = struct.unpack_from('<Q', buf, pos)[0], 8
    elif fmt == 0x0b: v, n = struct.unpack_from('<i', buf, pos)[0], 4
    elif fmt == 0x0c: v, n = struct.unpack_from('<q', buf, pos)[0], 8
    elif fmt == 0x02: v, n = struct.unpack_from('<H', buf, pos)[0], 2
    elif fmt == 0x0a: v, n = struct.unpack_from('<h', buf, pos)[0], 2
    else:
        raise ValueError(f'unsupported DW_EH_PE format {enc:#x}')
    if app == 0x10:  # pcrel
        v += base_vaddr + pos
    elif app == 0x30:  # datarel (eh_frame_hdr)
        pass
    return v & 0xffffffffffffffff, pos + n


def uleb(buf, pos):
    r = s = 0
    while True:
        b = buf[pos]; pos += 1
        r |= (b & 0x7f) << s; s += 7
        if b < 0x80:
            return r, pos


def fde_ranges(image, image_base, hdr_vaddr):
    """eh_frame_hdr -> FDE table -> (pc_begin, pc_range) for every function."""
    h = hdr_vaddr - image_base
    version, eh_ptr_enc, fde_count_enc, table_enc = image[h:h + 4]
    if version != 1:
        raise ValueError('eh_frame_hdr version')
    pos = h + 4
    _, pos = read_encoded(image, pos, eh_ptr_enc, image_base)
    count, pos = read_encoded(image, pos, fde_count_enc, image_base)
    cie_cache = {}
    out = []
    for i in range(count):
        # table: datarel from the start of eh_frame_hdr
        loc = struct.unpack_from('<i', image, pos + 8 * i)[0] + hdr_vaddr
        fde = struct.unpack_from('<i', image, pos + 8 * i + 4)[0] + hdr_vaddr
        f = fde - image_base
        length = struct.unpack_from('<I', image, f)[0]
        cie_off = struct.unpack_from('<I', image, f + 4)[0]
        cie = f + 4 - cie_off
        if cie not in cie_cache:
            # augmentation string -> FDE pointer encoding ('R')
            p = cie + 8 + 1  # length, id, version
            aug_end = image.index(0, p)
            aug = image[p:aug_end].decode('ascii'); p = aug_end + 1
            _, p = uleb(image, p)          # code align
            _, p = uleb(image, p)          # data align (sleb, the same length)
            p += 1                          # return register (version 1)
            enc = 0x00
            if aug.startswith('z'):
                _, p = uleb(image, p)
                for c in aug[1:]:
                    if c == 'R': enc = image[p]; p += 1
                    elif c == 'P': penc = image[p]; p += 1; _, p = read_encoded(image, p, penc, image_base)
                    elif c == 'L': p += 1
            cie_cache[cie] = enc
        enc = cie_cache[cie]
        begin, p = read_encoded(image, f + 8, enc, image_base)
        rng, _ = read_encoded(image, p, enc & 0x0f, image_base)
        if begin != loc:
            raise ValueError(f'FDE {i}: table {loc:#x} != pc_begin {begin:#x}')
        out.append((begin, rng))
    return out


def main(game, out):
    game, out = Path(game), Path(out)
    out.mkdir(parents=True, exist_ok=True)
    summary = {}
    for exe in EXES:
        path = game / exe
        if not path.is_file():
            print(f'skip {exe}: not found'); continue
        name = Path(exe).stem.replace('-ps4_Shipping', '')
        elf, header, ph, segments, missing = parse_self(path.read_bytes())
        loads = [p for p in ph if p['type'] in (PT_LOAD, PT_SCE_RELRO)]
        size = max(p['vaddr'] + p['memsz'] for p in loads)
        image = bytearray(size)
        for p in loads:
            image[p['vaddr']:p['vaddr'] + p['filesz']] = elf[p['offset']:p['offset'] + p['filesz']]
        # The full flat image (vaddr 0..size, no relocations): ps4lift needs it for switch tables
        # in .rodata and for the differential tests.
        (out / f'{name}.image.bin').write_bytes(image)
        segs = []
        for i, p in enumerate(ph):
            if p['type'] == PT_LOAD and p['flags'] & 1:
                fn = out / f'{name}.seg{i}.bin'
                fn.write_bytes(image[p['vaddr']:p['vaddr'] + p['filesz']])
                segs.append(dict(file=fn.name, vaddr=p['vaddr'], size=p['filesz']))
        funcs = []
        eh = next((p for p in ph if p['type'] == PT_GNU_EH_FRAME), None)
        if eh:
            funcs = fde_ranges(image, 0, eh['vaddr'])
        code = sum(s['size'] for s in segs)
        covered = sum(r for _, r in funcs)
        info = dict(exe=exe, entry=header[4], segments=segs, functions=len(funcs),
                    code_bytes=code, fde_bytes=covered)
        summary[name] = info
        (out / f'{name}.json').write_text(json.dumps(dict(info, ranges=funcs)))
        (out / f'{name}.ranges').write_text(''.join(f'{b:x} {r:x}\n' for b, r in funcs))
        print(f'{name:12s} code {code / 2**20:6.2f} MiB in {len(segs)} seg, {len(funcs):6d} FDE '
              f'functions covering {covered / max(code, 1) * 100:5.1f}%')
    (out / 'summary.json').write_text(json.dumps(summary, indent=1))


if __name__ == '__main__':
    main(*sys.argv[1:3])
