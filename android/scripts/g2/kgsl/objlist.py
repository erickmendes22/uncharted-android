import struct, sys
sys_argv = sys.argv
sys.argv = [sys_argv[0].replace("objlist.py", "snapparse.py"), sys_argv[1]]
src = open(sys.argv[0]).read().split("def show(")[0].replace('print("sections:", counts)', '').replace('print("  %-6s', 'pass #')
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    exec(src)
for kind, gpuaddr, dwords, doff in sorted(blobs, key=lambda b: b[1]):
    print("%-6s 0x%010x %6d dwords" % (kind, gpuaddr, dwords))
