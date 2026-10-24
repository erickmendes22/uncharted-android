import re, sys
# Summary of the last in-game stretch of a bb-probe.log: FPS and stage A decode time per frame.
path = re.sub(r'^/([a-zA-Z])/', r'\1:/', sys.argv[1])  # Git Bash path
text = open(path, encoding='utf-8', errors='replace').read()
K = int(sys.argv[2]) if len(sys.argv) > 2 else 12
fps = [float(x) for x in re.findall(r'Frame rate: ([\d.]+) FPS', text)][-K:]
idle = re.findall(r'SubmitDone -> GPU idle ([\d.]+) ms \(decode ([\d.]+), OnSubmit\+Flush ([\d.]+)', text)[-K:]
def mean(v):
    return sum(v) / len(v) if v else float('nan')
out = f'FPS {mean(fps):.1f} ({min(fps, default=0):.1f}-{max(fps, default=0):.1f}, {len(fps)} samples)'
if idle:
    out += (f'; SubmitDone->idle {mean([float(a) for a, _, _ in idle]):.1f} ms, decode '
            f'{mean([float(b) for _, b, _ in idle]):.1f}, OnSubmit+Flush {mean([float(c) for _, _, c in idle]):.1f}')
print(out)
