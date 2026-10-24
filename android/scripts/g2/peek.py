#!/usr/bin/env python3
# peek.py: guest memory of the running game through the port's BB_PEEK_FILE (probe.c).
#   from peek import Peek; p = Peek(); p.u64(0x1c08f00); p.read_many([(addr, len), ...])
# The game needs BB_PEEK_FILE=/storage/emulated/0/Android/data/dev.unchartedg2.game/files/peek.txt (port 0052).
# The request goes to <file>.new and is moved into place; the reply <file>.out is streamed back
# with adb exec-out (binary safe) and removed.
import glob
import struct
import subprocess

ADB = glob.glob(r'C:\Users\*\AppData\Local\Microsoft\WinGet\Packages\Google.PlatformTools_*\platform-tools\adb.exe')[0]
FILES = '/sdcard/Android/data/dev.unchartedg2.game/files'
REQUEST = FILES + '/peek.txt'


class Peek:
    def __init__(self, request=REQUEST):
        self.request = request
        self.cache = {}

    def read_many(self, ranges):
        """[(address, length)] -> [bytes or None] (None: not mapped)."""
        ranges = [(int(a), int(n)) for a, n in ranges]
        lines = ' '.join("'%x %x'" % (a, n) for a, n in ranges)
        r = self.request
        subprocess.run([ADB, 'shell', "printf '%%s\\n' %s > %s.new && mv %s.new %s" % (lines, r, r, r)],
                       check=True, capture_output=True)
        out = subprocess.run([ADB, 'exec-out', 'n=0; while [ ! -f %s.out ] && [ $n -lt 100 ]; do sleep 0.1; '
                              'n=$((n+1)); done; cat %s.out; rm -f %s.out' % (r, r, r)],
                             check=True, capture_output=True).stdout
        result, at = [], 0
        while at + 16 <= len(out):
            address, length, status = struct.unpack_from('<QII', out, at)
            at += 16
            if status == 0:
                result.append(out[at:at + length])
                at += length
            else:
                result.append(None)
        if len(result) != len(ranges):
            raise RuntimeError('peek: %d replies for %d requests' % (len(result), len(ranges)))
        return result

    def read(self, address, length):
        return self.read_many([(address, length)])[0]

    def u64(self, address):
        d = self.read(address, 8)
        return None if d is None else struct.unpack('<Q', d)[0]


def q(data, off):
    return struct.unpack_from('<Q', data, off)[0]


def qs(data, off):
    return struct.unpack_from('<q', data, off)[0]


def i32(data, off):
    return struct.unpack_from('<i', data, off)[0]
