"""KGSL GPU snapshot (/sys/class/kgsl/kgsl-3d0/snapshot/dump): sections, IBs and GPU objects.
python snapparse.py <snap.bin> [--find <hexaddr>[/<remaining dwords>] ...] [--dump <hexaddr> <n>]"""
import struct
import sys

NAMES = {0x0101: "OS", 0x0201: "REGS", 0x0202: "REGS_V2", 0x0301: "RB", 0x0302: "RB_V2",
         0x0401: "IB", 0x0402: "IB_V2", 0x0501: "INDEXED_REGS", 0x0502: "INDEXED_REGS_V2",
         0x0801: "ISTORE", 0x0901: "DEBUG", 0x0A01: "DEBUGBUS", 0x0B01: "GPU_OBJECT",
         0x0B02: "GPU_OBJECT_V2", 0x0E01: "MEMLIST", 0x0E02: "MEMLIST_V2", 0x1201: "SHADER",
         0x1202: "SHADER_V2", 0x1203: "SHADER_V3", 0x1501: "MVC", 0x1502: "MVC_V2",
         0x1503: "MVC_V3", 0x1601: "GMU", 0x1701: "GMU_MEMORY", 0x1801: "SIDE_DEBUGBUS",
         0x1901: "TRACE_BUFFER", 0x1A01: "EVENTLOG", 0xFFFF: "END"}

data = open(sys.argv[1], "rb").read()
magic, gpuid, chipid = struct.unpack_from("<III", data, 0)
print("magic 0x%x gpuid 0x%x chipid 0x%x" % (magic, gpuid, chipid))
off = 12
blobs = []  # (kind, gpuaddr, dwords, data offset)
counts = {}
while off + 8 <= len(data):
    smagic, sid, size = struct.unpack_from("<HHI", data, off)
    if smagic != 0xABCD:
        print("bad section magic at 0x%x" % off)
        break
    name = NAMES.get(sid, "0x%04x" % sid)
    counts[name] = counts.get(name, 0) + 1
    body = off + 8
    if sid == 0x0101:
        osid, seconds, pflags, plevel, pinterval, grpclk, busclk = struct.unpack_from("<iIIIIII", data, body)
        print("OS: seconds %u" % seconds)
    elif sid == 0x0402:
        gpuaddr, ptbase, dwords = struct.unpack_from("<QQQ", data, body)
        blobs.append(("IB", gpuaddr, dwords, body + 24))
    elif sid == 0x0401:
        gpuaddr, ptbase, dwords = struct.unpack_from("<IIi", data, body)
        blobs.append(("IB1", gpuaddr, dwords, body + 12))
    elif sid == 0x0B02:
        otype, gpuaddr, ptbase, dwords = struct.unpack_from("<iQQQ", data, body)
        blobs.append(("OBJ%d" % otype, gpuaddr, dwords, body + 28))
    elif sid == 0x0302:
        start, end, rbsize, wptr, rptr, count, tsq, tsr, gpuaddr, rid = struct.unpack_from(
            "<iiiiiiIIQI", data, body)
        print("RB id %u: gpuaddr 0x%x size %d wptr %d rptr %d count %d ts queued %u retired %u" % (
            rid, gpuaddr, rbsize, wptr, rptr, count, tsq, tsr))
        blobs.append(("RB%d" % rid, gpuaddr, count, body + 48))
    if sid == 0xFFFF or size == 0:
        break
    off += size
print("sections:", counts)
for kind, gpuaddr, dwords, doff in sorted(blobs, key=lambda b: b[1]):
    if kind.startswith("IB") or kind.startswith("RB"):
        print("  %-6s 0x%010x +0x%x dwords" % (kind, gpuaddr, dwords))


def show(addr, n_before=6, n_after=10):
    for kind, gpuaddr, dwords, doff in blobs:
        if gpuaddr <= addr < gpuaddr + dwords * 4:
            i = (addr - gpuaddr) // 4
            print("  0x%x is in %s 0x%x (+0x%x dwords of 0x%x)" % (addr, kind, gpuaddr, i, dwords))
            for j in range(max(0, i - n_before), min(dwords, i + n_after)):
                w = struct.unpack_from("<I", data, doff + j * 4)[0]
                mark = " <-" if j == i else ""
                print("     [%5x] %08x%s" % (j, w, mark))
            return True
    print("  0x%x: not in the snapshot" % addr)
    return False


args = sys.argv[2:]
while args:
    a = args.pop(0)
    if a == "--find":
        spec = args.pop(0)
        addr, _, rem = spec.partition("/")
        addr = int(addr, 16)
        for kind, gpuaddr, dwords, doff in blobs:
            if gpuaddr == addr and rem:
                # KGSL prints the IB base and the dwords left: the CP is at size - left.
                pos = gpuaddr + (dwords - int(rem, 16)) * 4
                print("fault position in %s: 0x%x" % (kind, pos))
                show(pos)
                break
        else:
            show(addr)
    elif a == "--dump":
        show(int(args.pop(0), 16), 0, int(args.pop(0)))
