"""Control packets of an IB1 in a KGSL snapshot: IB2 calls (address, size), THREAD_CONTROL, SET_MARKER,
COND_REG_EXEC, SET_DRAW_STATE, with the CP's fetch position (IB size - remaining dwords).
python ib1ctl.py <snap> <ib1 addr hex> [remaining dwords hex]"""
import struct
import sys

snap, ib = sys.argv[1], int(sys.argv[2], 16)
rem = int(sys.argv[3], 16) if len(sys.argv) > 3 else None
sys.argv = [sys.argv[0].replace("ib1ctl.py", "snapparse.py"), snap]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(open(sys.argv[0]).read().split("def show(")[0])
NAMES = {0x3f: "INDIRECT_BUFFER", 0x17: "THREAD_CONTROL", 0x65: "SET_MARKER", 0x47: "COND_REG_EXEC",
         0x43: "SET_DRAW_STATE", 0x26: "WAIT_FOR_IDLE", 0x46: "EVENT_WRITE", 0x63: "SET_MODE",
         0x10: "NOP", 0x5c: "CONTEXT_REG_BUNCH", 0x5d: "NON_CONTEXT_REG_BUNCH", 0x14: "WAIT_REG_MEM",
         0x3d: "MEM_WRITE", 0x22: "DRAW_INDX", 0x38: "DRAW_INDX_OFFSET", 0x33: "EXEC_CS",
         0x1f: "SKIP_IB2_ENABLE_LOCAL", 0x1d: "SKIP_IB2_ENABLE_GLOBAL", 0x6a: "SET_PSEUDO_REG",
         0x2a: "?0x2a", 0x31: "?0x31", 0x6b: "?0x6b", 0x4d: "MEM_TO_REG", 0x3e: "REG_TO_MEM",
         0x11: "WAIT_FOR_ME", 0x5b: "WAIT_TIMESTAMP", 0x34: "?0x34", 0x1a: "?0x1a"}
TC = {0: "?", 1: "BR", 2: "BV", 3: "BOTH"}
for kind, gpuaddr, dwords, doff in blobs:
    if gpuaddr != ib or not kind.startswith("IB"):
        continue
    w = struct.unpack_from("<%dI" % dwords, data, doff)
    pos = dwords - rem if rem is not None else None
    i = 0
    while i < dwords:
        h = w[i]
        t = h >> 28
        if t == 4:
            ln = h & 0x7f
            i += 1 + ln
            continue
        if t != 7:
            print("+%04x ??? %08x" % (i, h))
            i += 1
            continue
        op = (h >> 16) & 0x7f
        ln = h & 0x3fff
        mark = "   <== CP fetch position" if pos is not None and i <= pos < i + 1 + ln else ""
        if op in (0x3f, 0x17, 0x65, 0x47, 0x63, 0x1f, 0x1d, 0x6a, 0x43) or mark:
            p = w[i + 1:i + 1 + ln]
            extra = ""
            if op == 0x3f and ln >= 3:
                extra = "-> 0x%x, %d dwords" % (p[0] | (p[1] << 32), p[2] & 0xfffff)
            elif op == 0x17 and ln >= 1:
                extra = "thread %s, flags 0x%x" % (TC[p[0] & 3], p[0] & ~3)
            else:
                extra = " ".join("%08x" % v for v in p[:6])
            print("+%04x %-22s %s%s" % (i, NAMES.get(op, "0x%x" % op), extra, mark))
        if pos is not None and i > pos + 40:
            break
        i += 1 + ln
