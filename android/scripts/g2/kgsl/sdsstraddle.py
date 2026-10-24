import struct, sys
snap, ib, bo = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
sys.argv = [sys.argv[0].replace("sdsstraddle.py", "snapparse.py"), snap]
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(open(sys.argv[0]).read().split("def show(")[0])
for kind, gpuaddr, dwords, doff in blobs:
    if gpuaddr != ib or not kind.startswith("IB"): continue
    w = struct.unpack_from("<%dI" % dwords, data, doff)
    i = 0; draw = 0
    while i < dwords:
        h = w[i]; t = h >> 28
        ln = (h & 0x7f) if t == 4 else (h & 0x3fff) if t == 7 else 0
        op = (h >> 16) & 0x7f if t == 7 else None
        if op in (0x38, 0x22): draw += 1
        start = ib - bo + i * 4; end = start + (1 + ln) * 4 - 1
        marks = [str(b) for b in (512, 1024, 2048, 4096) if start // b != end // b]
        if marks and (op == 0x43 or ln >= 16):
            print("+0x%04x BO 0x%05x..0x%05x %-6s len %3d before draw %2d: crosses %s" % (
                i, start, end, ("SDS" if op == 0x43 else ("op%02x" % op if op is not None else "pkt4")), ln, draw + 1, ",".join(marks)))
        i += 1 + ln
        if draw > 20: break
