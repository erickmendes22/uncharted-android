#!/usr/bin/env python3
# job3_state.py [counter ...]: U3's job system state in the running game (peek.py): the manager
# (0x1c08f00), per level the ready list (job batches) and the resume list (job entries whose waits
# are over), the per-worker counts the idle workers check, the wait list (job entries waiting for
# counters) and the deferred batches, with the current values of the counters involved. Counters
# given on the command line (r15 of the main thread in WaitForCounter) are shown too.
# Layout from big3's code: AddToReadyList @0x11a57d0, WakeWaiters @0x11a5c20, idle loop @0x11a6470.
# A job entry (0x80 bytes): +0 function, +8 data, +0x10 its counter, +0x18/+0x20 the counters it
# waits for, +0x28/+0x30 their targets, +0x38 flags (bits 0-1 / 2-3: mode 0 '==', 1 '<=' of the
# two waits; 0x100 keep the wait), +0x40 worker mask, +0x48 level.
import sys
from peek import Peek, q, qs, i32

MANAGER = 0x1c08f00
LEVELS, LEVEL_SIZE = 8, 0x2078

p = Peek()
ctx = p.u64(MANAGER)
C = p.read(ctx, 0x5000)
nworkers = q(C, 0x100)
print('manager %#x: workers %d, flag58 %d, sleep %d, active %d' % (ctx, nworkers, C[0x58], C[0x59], qs(C, 0x4f50)))
for w in range(nworkers):
    slot = q(C, 0x780 + w * 0x80)
    if slot:
        print('  worker %d slot %s' % (w, ' '.join('%x' % q(C, 0x780 + w * 0x80 + o) for o in range(0, 0x50, 8))))
levels = q(C, 0x4f10)
L = p.read(levels, LEVELS * LEVEL_SIZE)

counters = {}
def note(c):
    if c:
        counters.setdefault(c, None)

batches, resume = [], []
for lv in range(LEVELS):
    b = lv * LEVEL_SIZE
    count = q(L, b)
    pw = [qs(L, b + 8 + w * 8) for w in range(nworkers)]
    rcount = q(L, b + 0x2038)
    rpw = [qs(L, b + 0x2040 + w * 8) for w in range(nworkers)]
    rarr = q(L, b + 0x2070)
    if count or rcount or any(pw) or any(rpw):
        print('level %d: ready %d per-worker %s | resume %d per-worker %s' % (lv, count, pw, rcount, rpw))
    for i in range(min(count, 0x400)):
        batches.append((lv, q(L, b + 0x38 + i * 8)))
    if rcount:
        R = p.read(rarr, min(rcount, 0x100) * 0x80)
        for i in range(min(rcount, 0x100)):
            resume.append((lv, R[i * 0x80:(i + 1) * 0x80]))

wait_arr, wait_count = q(C, 0x4ed8), q(C, 0x4ee0)
waits = []
if wait_count:
    W = p.read(wait_arr, min(wait_count, 0x800) * 0x80)
    waits = [W[i * 0x80:(i + 1) * 0x80] for i in range(min(wait_count, 0x800))]

deferred, node = [], q(C, 0x4ed0)
while node and len(deferred) < 512:
    N = p.read(node, 0x10)
    if N is None:
        print('deferred node %#x unreadable' % node)
        break
    deferred.append(q(N, 0))
    node = q(N, 8)

bobj = {}
addrs = sorted(set([a for _, a in batches] + deferred))
for a, d in zip(addrs, p.read_many([(a, 0x60) for a in addrs]) if addrs else []):
    bobj[a] = d
    if d:
        note(q(d, 0x20))
for e in waits + [r for _, r in resume]:
    for o in (0x10, 0x18, 0x20):
        note(q(e, o))
for a in sys.argv[1:]:
    note(int(a, 16))
cl = sorted(counters)
for c, d in zip(cl, p.read_many([(c, 0x40) for c in cl]) if cl else []):
    counters[c] = d

def cval(c):
    d = counters.get(c)
    return '-' if not c else 'unreadable' if d is None else '%d' % qs(d, 0x20)

def batch_text(a):
    d = bobj.get(a)
    if d is None:
        return '%#x unreadable' % a
    dep = q(d, 0x20)
    return ('%#x: words %s | left %d, dep %#x (now %s) %s %d, mask %#x, level %d' %
            (a, ' '.join('%x' % q(d, o) for o in range(0, 0x20, 8)), i32(d, 8), dep, cval(dep),
             '<=' if i32(d, 0x30) == 1 else '==', qs(d, 0x28), q(d, 0x38), i32(d, 0x4c)))

def entry_text(e):
    flags = q(e, 0x38)
    parts = []
    for co, to, mode in ((0x18, 0x28, flags & 3), (0x20, 0x30, (flags >> 2) & 3)):
        c = q(e, co)
        if c:
            ok = {0: lambda v, t: v == t, 1: lambda v, t: v <= t}.get(mode)
            v = counters.get(c)
            sat = '' if v is None or ok is None else (' MET' if ok(qs(v, 0x20), qs(e, to)) else '')
            parts.append('%#x (now %s) %s %d%s' % (c, cval(c), {0: '==', 1: '<='}.get(mode, 'mode%d' % mode),
                                                   qs(e, to), sat))
    return 'fn %#x data %#x counter %#x (now %s), waits %s, flags %#x, mask %#x, level %d' % (
        q(e, 0), q(e, 8), q(e, 0x10), cval(q(e, 0x10)), ' and '.join(parts) or 'nothing', flags, q(e, 0x40),
        i32(e, 0x48))

print('wait list %d, deferred batches %d' % (len(waits), len(deferred)))
for lv, a in batches:
    print('ready L%d %s' % (lv, batch_text(a)))
for a in deferred:
    print('deferred %s' % batch_text(a))
for e in waits:
    print('wait   ' + entry_text(e))
for lv, e in resume:
    print('resume L%d ' % lv + entry_text(e))
for a in sys.argv[1:]:
    c = int(a, 16)
    d = counters.get(c)
    print('counter %#x: value %s, words %s' % (c, cval(c), ' '.join('%x' % q(d, o) for o in range(0, 0x40, 8)) if d else '-'))
