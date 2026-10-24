import struct, sys
snap, ib, at = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
rem = int(sys.argv[4], 16)
sys.argv = [sys.argv[0].replace("sdspkt.py", "snapparse.py"), snap]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(open(sys.argv[0]).read().split("def show(")[0])
for kind, gpuaddr, dwords, doff in blobs:
    if gpuaddr != ib or not kind.startswith("IB"):
        continue
    w = struct.unpack_from("<%dI" % dwords, data, doff)
    pos = dwords - rem
    h = w[at]; ln = h & 0x3fff
    print("packet at +0x%x: %08x len %d, fetch position +0x%x" % (at, h, ln, pos))
    for k in range(at + 1, at + 1 + ln, 3):
        d0 = w[k]; a = w[k + 1] | (w[k + 2] << 32)
        flags = []
        for bit, name in ((16, "DIRTY?"), (17, "DISABLE"), (18, "DIS_ALL"), (19, "LOAD_IMMED"), (20, "BINNING"), (21, "GMEM"), (22, "SYSMEM")):
            if d0 & (1 << bit): flags.append(name)
        mark = "  <== fetch stops in this entry" if k <= pos < k + 3 else ""
        print("  +0x%x g%02x n%-3d %-28s @0x%x%s" % (k, (d0 >> 24) & 0x1f, d0 & 0xffff, ",".join(flags), a, mark))
