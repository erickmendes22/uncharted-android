"""IB1 calls in a KGSL ringbuffer of a snapshot, with their place relative to rptr/wptr, to see whether
a command buffer (its IB1 address) is queued again while an earlier submission of it is not done.
python rbibs.py <snap> [rb id, default: the one with pending work]"""
import struct
import sys

snap = sys.argv[1]
want = int(sys.argv[2]) if len(sys.argv) > 2 else None
data = open(snap, "rb").read()
off = 12
rbs = []
while off + 8 <= len(data):
    m, sid, size = struct.unpack_from("<HHI", data, off)
    if m != 0xABCD or size == 0:
        break
    if sid == 0x0302:
        body = off + 8
        start, end, rbsize, wptr, rptr, count, tsq, tsr, gpuaddr, rid = struct.unpack_from(
            "<iiiiiiIIQI", data, body)
        rbs.append((rid, gpuaddr, rbsize, wptr, rptr, count, tsq, tsr, body + 48))
    if sid == 0xFFFF:
        break
    off += size
for rid, gpuaddr, rbsize, wptr, rptr, count, tsq, tsr, doff in rbs:
    if want is not None and rid != want:
        continue
    if want is None and wptr == rptr:
        continue
    w = struct.unpack_from("<%dI" % count, data, doff)
    print("RB %d: rptr %d wptr %d, timestamps queued %d retired %d" % (rid, rptr, wptr, tsq, tsr))
    # Walk the whole ring once from wptr (oldest data) around to wptr (newest).
    i = 0
    seen = {}
    calls = []
    pos = wptr
    steps = 0
    while steps < count:
        h = w[pos % count]
        t = h >> 28
        if t == 7:
            op = (h >> 16) & 0x7f
            ln = h & 0x3fff
            if op in (0x3f, 0x34) and ln >= 3:  # INDIRECT_BUFFER / INDIRECT_BUFFER_PFE
                a = w[(pos + 1) % count] | (w[(pos + 2) % count] << 32)
                n = w[(pos + 3) % count] & 0xfffff
                rel = (pos - rptr) % count
                state = "pending" if rel < (wptr - rptr) % count else "done"
                calls.append((pos % count, a, n, state))
            step = 1 + ln
        elif t == 4:
            step = 1 + (h & 0x7f)
        else:
            step = 1
        pos += step
        steps += step
    for p, a, n, state in calls:
        mark = ""
        if a in seen:
            mark = "  <== IB1 also at %d (%s)" % seen[a]
        seen[a] = (p, state)
        print("  rb[%4d] IB 0x%x %5d dwords %s%s" % (p, a, n, state, mark))
