#!/usr/bin/env python3
"""aba.py <frames dir> [cells_x=60] [cells_y=34] [top=12]: an A-B-A flicker score over the
frames of a screen recording (f-000001.png ..., greyscale, full resolution; see flick.sh).

For every frame F with its neighbours P and N, per pixel:
    score = max(0, min(|F - P|, |F - N|) - |P - N|)
A pixel that changes and changes back scores high; motion (P != N) is subtracted away. The
frame is split into cells (cells_x by cells_y) and a cell's value is the mean of its pixels,
so fine-grained alternation (shimmer on a jacket's highlights) counts as well as a flash.
Prints a per-frame series of the best cell and the top frames with the cell's position, so
the suspect region can be cropped from the frames and looked at (2026-10-23: U3's jacket
scored 10-15 on 32x31-pixel cells, the rest of the frame under 3)."""
import glob
import os
import sys

from PIL import Image, ImageChops

frames_dir = sys.argv[1]
cells_x = int(sys.argv[2]) if len(sys.argv) > 2 else 60
cells_y = int(sys.argv[3]) if len(sys.argv) > 3 else 34
top = int(sys.argv[4]) if len(sys.argv) > 4 else 12
files = sorted(glob.glob(os.path.join(frames_dir, 'f-*.png')))
if len(files) < 3:
    sys.exit("fewer than three frames")
prev = Image.open(files[0]).convert('L')
cur = Image.open(files[1]).convert('L')
results = []
series = []
for index in range(2, len(files)):
    nxt = Image.open(files[index]).convert('L')
    d_prev = ImageChops.difference(prev, cur)
    d_next = ImageChops.difference(cur, nxt)
    d_around = ImageChops.difference(prev, nxt)
    score = ImageChops.subtract(ImageChops.darker(d_prev, d_next), d_around)
    cells = score.resize((cells_x, cells_y), Image.BOX)
    pixels = cells.load()
    best = (0, 0, 0)
    for y in range(cells_y):
        for x in range(cells_x):
            value = pixels[x, y]
            if value > best[0]:
                best = (value, x, y)
    mean = sum(cells.getdata()) / (cells_x * cells_y)
    results.append((best[0], index, best[1], best[2], mean))
    series.append(best[0])
    prev, cur = cur, nxt
width, height = Image.open(files[0]).size
print(f"frames {len(files)} of {width}x{height}, cells {width // cells_x}x{height // cells_y} px")
print("per-frame best cell score:", ' '.join(str(v) for v in series))
for value, index, x, y, mean in sorted(results, reverse=True)[:top]:
    print(f"frame {index:4d}: cell score {value:3d} at cell ({x},{y}) = px "
          f"({x * width // cells_x},{y * height // cells_y}), frame mean {mean:.2f}")
