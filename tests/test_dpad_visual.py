#!/usr/bin/env python3
"""Uncharted touch D-pad appearance: four independent PlayStation-style arms.

The drawn control must match its 58-unit hit area and retain per-direction
pressed feedback and the existing, optional classic-layout buttons.
"""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/"android/scripts/g2/game_app/TouchControllerOverlay.java").read_text()
start=s.index('private void actionDpad(Canvas c)')
end=s.index('private void actionStick(',start)
d=s[start:end]
assert 'DPAD_ARMS' in s and 'DPAD_DIRECTIONS' in s
assert 'pressed.contains(DPAD_DIRECTIONS[i])' in d
assert 'c.drawRoundRect(x0,y0,x1,y1' in d
assert 'c.drawRoundRect(-12f*s,-12f*s,12f*s,12f*s' in d
assert 'c.drawLine(-7f*s,-29f*s,0,-37f*s,paint)' in d
assert 'c.drawLine(29f*s,-7f*s,37f*s,0,paint)' in d
assert 'c.drawLine(-7f*s,29f*s,0,37f*s,paint)' in d
assert 'c.drawLine(-29f*s,-7f*s,-37f*s,0,paint)' in d
assert 'c.drawCircle(p.x,p.y,r,paint)' not in d
assert 'actionDistance(x,y,dpad)<58*sizeMultiplier' in s
assert 'DpadResolver.resolve(dx,p.y-c.y,sizeMultiplier)' in s
assert 'circle(c,"up",39,"▲"' in s
print("PASS: four beveled arrow arms, independent pressed states, no circular D-pad, classic retained")
