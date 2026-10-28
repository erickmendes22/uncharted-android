"""OS-level regression for the installer's seekable parent-fd path (no PKG copy)."""
import mmap, os, pathlib, subprocess, sys, tempfile
with tempfile.TemporaryDirectory() as tmp:
    p=pathlib.Path(tmp)/'provider.pkg'
    p.write_bytes(b'\x7fCNT'+bytes(range(256))*64)
    with p.open('rb') as fd:
        p.unlink() # Provider exposes a readable fd without any ordinary pathname.
        path=f'/proc/{os.readlink('/proc/self')}/fd/{fd.fileno()}'
        code='''import mmap,sys
with open(sys.argv[1],'rb') as f:
 assert f.read(4)==b'\\x7fCNT'
 f.seek(12000);assert f.read(4)==bytes([220,221,222,223])
 with mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:assert m[:4]==b'\\x7fCNT'
'''
        for phase in ('info','extract'):
            with open(path,'rb') as stream:
                subprocess.run([sys.executable,'-c',code,'/proc/self/fd/0'],check=True,close_fds=True,stdin=stream)
print('PASS: sequential info/extract children can seek/mmap a retained provider fd without copying PKG')
