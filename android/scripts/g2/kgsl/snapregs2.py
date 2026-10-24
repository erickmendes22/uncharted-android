import struct, sys
data = open(sys.argv[1], "rb").read()
WANT = {0x821: "CP_HW_FAULT", 0x824: "CP_PROTECT_STATUS", 0x928: "IB1_BASE_LO", 0x929: "IB1_BASE_HI",
        0x92a: "IB1_REM", 0x92b: "IB2_BASE_LO", 0x92c: "IB2_BASE_HI", 0x92d: "IB2_REM",
        0x806: "RB_RPTR", 0x807: "RB_WPTR", 0x93a: "ROQ_IB1_STATUS", 0x93b: "ROQ_IB2_STATUS",
        0x908: "SQE_STAT_ADDR", 0x800: "CP_RB_BASE_LO"}
off = 12; idx = 0
while off + 8 <= len(data):
    m, sid, size = struct.unpack_from("<HHI", data, off)
    if m != 0xABCD or size == 0: break
    if sid == 0x0202:
        body = data[off + 8: off + size]
        w = struct.unpack_from("<%dI" % (len(body) // 4), body)
        regs = {}
        i = 0
        while i < len(w):
            e = w[i]
            if e & 0x80000000:
                if i + 1 >= len(w): break
                regs[e & 0x7fffffff] = w[i + 1]; i += 2
            else:
                if i + 1 >= len(w): break
                cnt = w[i + 1]
                if cnt > 100000 or i + 2 + cnt > len(w): break
                for k in range(cnt): regs[e + k] = w[i + 2 + k]
                i += 2 + cnt
        hit = {WANT[r]: "0x%x" % regs[r] for r in WANT if r in regs}
        if hit:
            print("section %d: %s" % (idx, hit))
        idx += 1
    if sid == 0xFFFF: break
    off += size
