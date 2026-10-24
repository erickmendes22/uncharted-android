"""Walk one IB in a KGSL snapshot and print packets; CP_CONTEXT_REG_BUNCH pairs in full.
python crbdump.py <snap> <ib addr hex> [max packets]"""
import struct, sys
snap, ib = sys.argv[1], int(sys.argv[2], 16)
maxp = int(sys.argv[3]) if len(sys.argv) > 3 else 400
sys.argv = [sys.argv[0].replace("crbdump.py", "snapparse.py"), snap]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(open(sys.argv[0]).read().split("def show(")[0])
OPS = {0x5c: "CONTEXT_REG_BUNCH", 0x43: "SET_DRAW_STATE", 0x65: "SET_MARKER", 0x17: "THREAD_CONTROL",
       0x3f: "INDIRECT_BUFFER", 0x38: "DRAW_INDX_OFFSET", 0x47: "COND_REG_EXEC", 0x10: "NOP",
       0x46: "EVENT_WRITE", 0x26: "WAIT_FOR_IDLE", 0x31: "SKIP_IB2_ENABLE_GLOBAL", 0x2f: "?", 0x63: "SET_MODE"}
for kind, gpuaddr, dwords, doff in blobs:
    if gpuaddr != ib or not kind.startswith("IB"): continue
    w = struct.unpack_from("<%dI" % dwords, data, doff); i = 0; n = 0
    while i < dwords and n < maxp:
        h = w[i]; t = h >> 28
        if t == 4:
            reg = (h >> 8) & 0x3ffff; ln = h & 0x7f
            print("+%04x pkt4 reg 0x%05x x%d: %s" % (i, reg, ln, " ".join("%08x" % v for v in w[i+1:i+1+ln][:8])))
        elif t == 7:
            op = (h >> 16) & 0x7f; ln = h & 0x3fff
            print("+%04x pkt7 %-18s len %d" % (i, OPS.get(op, "0x%x" % op), ln))
            if op == 0x5c:
                for k in range(i + 1, i + 1 + ln, 2):
                    print("        0x%05x = 0x%08x" % (w[k], w[k + 1]))
        else:
            print("+%04x ??? %08x" % (i, h)); ln = 0
        i += 1 + ln; n += 1
