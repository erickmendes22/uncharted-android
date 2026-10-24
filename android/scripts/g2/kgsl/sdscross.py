"""Per draw of an IB2: draw state groups set since the previous draw, and those crossing a 4 KiB page
(or 64 KiB) boundary.   python sdscross.py <snap> <ib2 hex> [remaining hex]"""
import struct
import sys

snap, ib2 = sys.argv[1], int(sys.argv[2], 16)
rem = int(sys.argv[3], 16) if len(sys.argv) > 3 else None
sys.argv = [sys.argv[0].replace("sdscross.py", "snapparse.py"), snap]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(open(sys.argv[0]).read().split("def show(")[0])
DRAW = (0x38, 0x22, 0x28, 0x29, 0x2a)
for kind, gpuaddr, dwords, doff in blobs:
    if gpuaddr != ib2 or not kind.startswith("IB"):
        continue
    w = struct.unpack_from("<%dI" % dwords, data, doff)
    stop = dwords - rem if rem is not None else dwords
    i, draw, pending = 0, 0, []
    while i < dwords:
        h = w[i]
        t = h >> 28
        ln = (h & 0x7f) if t == 4 else (h & 0x3fff) if t == 7 else 0
        op = (h >> 16) & 0x7f if t == 7 else None
        if op == 0x43:
            for k in range(i + 1, i + ln - 1, 3):
                d0 = w[k]
                a = w[k + 1] | (w[k + 2] << 32)
                n = d0 & 0xffff
                if d0 & (3 << 17) or not n:
                    continue
                g = (d0 >> 24) & 0x1f
                end = a + 4 * n - 1
                cross = []
                if (a >> 12) != (end >> 12):
                    cross.append("4K")
                if (a >> 16) != (end >> 16):
                    cross.append("64K")
                pending.append("g%02x@%x+%d%s" % (g, a & 0xfffff, n, ("[" + ",".join(cross) + "]") if cross else ""))
        if op in DRAW:
            draw += 1
            crossing = [p for p in pending if "[" in p]
            print("draw %2d at +0x%04x%s: %d groups set, crossing: %s" % (
                draw, i, "  <== CP position before this" if i >= stop and draw and (i - stop) < 80 else "",
                len(pending), " ".join(crossing) or "-"))
            pending = []
        if i >= stop + 200:
            break
        i += 1 + ln
