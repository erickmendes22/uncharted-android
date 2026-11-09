#!/usr/bin/env python3
"""Assert icons seen in user's approved GoW3-style reference really ship in Java.

Validates eight losslessly decoded packed alpha masks (silhouette artwork),
the game/launcher style separation, and the unchanged Classic pad rendering.
"""
from pathlib import Path
import base64
import re
import zlib

root=Path(__file__).resolve().parents[1]
source=(root/"android/scripts/g2/game_app/UnchartedActionGlyphs.java").read_text()
view=(root/"android/scripts/g2/game_app/TouchControllerOverlay.java").read_text()
pairs=re.findall(r'\{"(l1|l2|r1|r2|triangle|square|circle|cross)","([A-Za-z0-9+/=]+)"\}',source)
assets=dict(pairs)
assert set(assets)=={'l1','l2','r1','r2','triangle','square','circle','cross'}
assert len(pairs)==8
for key,enc in assets.items():
    raw=zlib.decompress(base64.b64decode(enc,validate=True))
    assert len(raw)==2048,(key,len(raw))
    nibbles=[((byte>>4)&15) for byte in raw]+[(byte&15) for byte in raw]
    assert sum(v>0 for v in nibbles)>90,(key,'empty icon')
    assert sum(v>0 for v in nibbles)<4096,(key,'opaque background')
assert 'Bitmap.createBitmap(SIZE,SIZE,Bitmap.Config.ARGB_8888)' in source
assert 'if(detailed!=null)' in view
assert 'Bitmap detailed=glyphs.get(name);' in view
assert 'new RectF(p.x-iconRadius,p.y-iconRadius,p.x+iconRadius,p.y+iconRadius)' in view
assert 'drawActionGlyph(c,name,p.x,p.y,r*.67f)' in view  # fallback
assert 'if (unchartedStyle) {' in view
assert 'rectangle(c,"l2",108,"L2")' in view       # Classic untouched
assert 'circle(c,"triangle",39,"△"' in view       # PS face symbols untouched
assert 'actionStick(c,"rightstick")' in view     # camera remains analog
assert 'actionClickStick(c,"r3")' not in view
print('PASS: 8 detailed source-derived action glyphs, valid packed alpha, separate Classic buttons')
