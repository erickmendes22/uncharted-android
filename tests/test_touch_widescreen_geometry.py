#!/usr/bin/env python3
"""Touch UI ergonomics on 16:9 and 20:9 phones, without an Android build.

The Uncharted action layout must use the FULL display width while preserving
round buttons, matching hit-testing and camera axes. Classic coordinates are
not changed.
"""
import math
import re
from pathlib import Path

root=Path(__file__).resolve().parents[1]
s=(root/"android/scripts/g2/game_app/TouchControllerOverlay.java").read_text()
assert 'ACTION_LAYOUT_VERSION=4' in s
assert 'getWidth()/(1280f*scale())' in s
assert '640f+(x-640f)*actionHorizontalScale()' in s
assert '640f+(x-640f)/actionHorizontalScale()' in s
assert 'return unchartedStyle?actionLogicalX(x):x;' in s
assert 'PointF p=actionCenter(name)' in s
assert 'actionDistance(x,y,p)<radius*sizeMultiplier' in s
assert 'actionDistance(x,y,dpad)<58*sizeMultiplier' in s
assert 'PointF p=actionDpadCenter()' in s
assert 'float toolbarX=screenLayoutX(e,at);' in s
assert 'float dx=actionDisplayX(q.x)-p.x;' in s
assert '56f*sizeMultiplier/(unchartedStyle?actionHorizontalScale():1f)' in s
assert 'circle(c,"triangle",39,"△"' in s  # Classic untouched
assert 'actionStick(c,"rightstick")' in s
assert 'actionStick(c,"leftstick")' in s
assert 'actionClickStick(c,"r3")' not in s

start=s.index('private static final float[][] ACTION_DEFAULT=')
end=s.index('};',start)
nums=[tuple(map(int,match)) for match in
      re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*\}',s[start:end])]
assert len(nums)==18,nums
names=['l2','l1','touchpad','options','r1','r2',
       'up','left','right','down','triangle','circle','cross','square',
       'leftstick','l3','r3','rightstick']
xy=dict(zip(names,nums))
assert xy['leftstick'][0] < 0.15*1280
assert 0.62*1280 < xy['rightstick'][0] < 0.74*1280
assert xy['triangle'][0] > 0.80*1280
# Actual in-game action grouping: deliberately NOT the gamepad's fixed diamond.
assert xy['triangle'][1] < xy['square'][1] < xy['circle'][1] < xy['cross'][1]
assert xy['square'][0] < xy['cross'][0] < xy['triangle'][0] < xy['circle'][0]
assert xy['rightstick'][0] < xy['square'][0]
assert xy['rightstick'][1] > xy['square'][1]
assert xy['circle'][0] - xy['square'][0] > 160
assert xy['circle'][0] > 0.89*1280
assert xy['leftstick'][1] > 0.7*720
assert xy['rightstick'][1] > 0.7*720

# User image is 2048x926 (20:9-ish). Check a precise inverse for both input
# hitboxes and rendering: circular hit areas should track the drawn buttons.
buttons={'l2':46,'l1':46,'touchpad':34,'options':34,'r1':46,'r2':46,
         'triangle':46,'square':46,'circle':46,'cross':46,
         'dpad':45,'leftstick':80,'rightstick':80}
xy['dpad']=(sum(xy[n][0] for n in ('up','left','right','down'))/4,
            sum(xy[n][1] for n in ('up','left','right','down'))/4)
for width,height in [(1280,720),(1672,941),(2048,926),(2400,1080)]:
    scale=min(width/1280,height/720)
    offset=(width-1280*scale)/2
    horizontal=max(1.0,width/(1280*scale))
    positions={}
    for name,radius in buttons.items():
        x,y=xy[name]
        warped=640+(x-640)*horizontal
        sx=offset+scale*warped
        sy=(height-720*scale)/2+scale*y
        # Inverse mirrors layoutX -> actionLogicalX.
        recovered=640+(warped-640)/horizontal
        assert abs(recovered-x)<1e-6,(name,width,recovered,x)
        assert abs(sx-width*x/1280)<1e-5,(name,width,sx)
        assert 0 < sx < width and 0 < sy < height,(name,width,height,sx,sy)
        positions[name]=(sx,sy,radius*scale)
    # No interactive circles overlap except at intentionally adjacent areas.
    for name,(x,y,radius) in positions.items():
        for other,(ox,oy,oradius) in positions.items():
            if name >= other:continue
            dist=math.hypot(x-ox,y-oy)
            assert dist > (radius+oradius)*.85, (width,height,name,other,dist)
print('PASS: full-width touch placement on 16:9/20:9, circular hitboxes, classic layout preserved')
