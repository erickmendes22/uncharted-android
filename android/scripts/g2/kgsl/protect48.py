import struct, sys
data = open(sys.argv[1], "rb").read(); target = int(sys.argv[2], 16)
off = 12; tables = []
while off + 8 <= len(data):
    m, sid, size = struct.unpack_from("<HHI", data, off)
    if m != 0xABCD or size == 0: break
    if sid == 0x0202:
        body = data[off + 8: off + size]; w = struct.unpack_from("<%dI" % (len(body) // 4), body); i = 0; regs = {}
        while i + 1 < len(w):
            e = w[i]
            if e & 0x80000000: regs[e & 0x7fffffff] = w[i + 1]; i += 2
            else:
                cnt = w[i + 1]
                if cnt > 100000 or i + 2 + cnt > len(w): break
                for k in range(cnt): regs[e + k] = w[i + 2 + k]
                i += 2 + cnt
        if 0x850 in regs: tables.append(regs)
    if sid == 0xFFFF: break
    off += size
for regs in tables:
    print("CP_PROTECT_CNTL 0x%x" % regs.get(0x84f, 0))
    for r in range(0x850, 0x880):
        v = regs.get(r)
        if v is None: continue
        base = v & 0x3ffff; cnt = (v >> 18) & 0x1fff
        hit = base <= target <= base + cnt
        if True:
            print("  [%2d] 0x%08x base 0x%05x..0x%05x read %d%s" % (r - 0x850, v, base, base + cnt, v >> 31, "  <== covers" if hit else ""))
