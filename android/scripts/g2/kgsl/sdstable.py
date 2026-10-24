"""The CP draw state table at a fault: replays CP_SET_DRAW_STATE packets of the IB1 and of the IB2s it
calls (the faulting IB2 up to the CP position), then checks each active group whose memory is in the
snapshot: do its PM4 packets end exactly at the group's size, any zero or unknown headers?
python sdstable.py <snap> <ib1 hex> <ib2 hex> <ib2 remaining hex>"""
import struct
import sys

snap = sys.argv[1]
ib1, ib2, rem2 = int(sys.argv[2], 16), int(sys.argv[3], 16), int(sys.argv[4], 16)
sys.argv = [sys.argv[0].replace("sdstable.py", "snapparse.py"), snap]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(open(sys.argv[0]).read().split("def show(")[0])

objs = [(k, a, n, o) for (k, a, n, o) in blobs if k.startswith("OBJ") or k.startswith("IB")]


def read(addr, count):
    """dwords at a GPU address from any captured object, or None."""
    for kind, gpuaddr, dwords, doff in objs:
        if gpuaddr <= addr and addr + count * 4 <= gpuaddr + dwords * 4:
            i = (addr - gpuaddr) // 4
            return list(struct.unpack_from("<%dI" % count, data, doff + i * 4))
    return None


GROUPS = ["PROGRAM_CONFIG", "VS", "VS_BINNING", "HS", "DS", "GS", "GS_BINNING", "VPC", "FS", "VB",
          "CONST", "DESC_SETS", "DESC_SETS_LOAD", "VS_PARAMS", "FS_PARAMS", "INPUT_ATT_GMEM",
          "INPUT_ATT_SYSMEM", "LRZ_AND_DEPTH_PLANE", "PRIM_MODE_GMEM"]
DYN = ["VIEWPORT", "SCISSOR", "RAST", "DEPTH_BIAS", "BLEND_CONSTANTS", "DS", "RB_DEPTH_CNTL",
       "SAMPLE_LOCATIONS", "VB_STRIDE", "BLEND", "VERTEX_INPUT", "PATCH_CONTROL_POINTS",
       "PRIM_MODE_SYSMEM/FSR"]


def gname(g):
    if g < len(GROUPS):
        return GROUPS[g]
    if g - len(GROUPS) < len(DYN):
        return "DYN " + DYN[g - len(GROUPS)]
    return "group %d" % g


table = {}
draws = 0


def walk(addr, dwords, stop_at=None, depth=0):
    global draws
    w = read(addr, dwords)
    if w is None:
        print("IB 0x%x not captured" % addr)
        return
    i = 0
    while i < dwords:
        if stop_at is not None and i >= stop_at:
            return
        h = w[i]
        t = h >> 28
        if t == 4:
            i += 1 + (h & 0x7f)
            continue
        if t != 7:
            print("  bad header %08x at 0x%x" % (h, addr + i * 4))
            return
        op = (h >> 16) & 0x7f
        ln = h & 0x3fff
        p = w[i + 1:i + 1 + ln]
        if op == 0x43:  # SET_DRAW_STATE
            for k in range(0, ln - 2, 3):
                d0 = p[k]
                a = p[k + 1] | (p[k + 2] << 32)
                if d0 & (1 << 18):  # DISABLE_ALL_GROUPS
                    table.clear()
                    continue
                g = (d0 >> 24) & 0x1f
                if d0 & (1 << 17):
                    table.pop(g, None)
                else:
                    table[g] = (a, d0 & 0xffff, (d0 >> 20) & 7)
        elif op == 0x3f and depth == 0 and ln >= 3:  # INDIRECT_BUFFER from the IB1
            target = p[0] | (p[1] << 32)
            n = p[2] & 0xfffff
            if target == ib2:
                walk(target, n, n - rem2, depth + 1)
                return True
            walk(target, n, None, depth + 1)
        elif op in (0x38, 0x22, 0x28, 0x29, 0x2a):
            draws += 1
        i += 1 + ln
    return False


walk(ib1, read_len := next(n for (k, a, n, o) in blobs if a == ib1 and k.startswith("IB")))
print("draws before the CP position: %d; active groups:" % draws)
for g in sorted(table):
    a, n, modes = table[g]
    w = read(a, n) if n else []
    if w is None:
        print("  g%02x %-28s 0x%x n%-4d (not captured)" % (g, gname(g), a, n))
        continue
    i, bad = 0, []
    while i < n:
        h = w[i]
        t = h >> 28
        if t == 4:
            i += 1 + (h & 0x7f)
        elif t == 7:
            i += 1 + (h & 0x3fff)
        else:
            bad.append("header %08x at +%d" % (h, i))
            break
    status = "ok" if i == n and not bad else ("ends at %d of %d %s" % (i, n, " ".join(bad)))
    print("  g%02x %-28s 0x%x n%-4d modes %d: %s" % (g, gname(g), a, n, modes, status))
