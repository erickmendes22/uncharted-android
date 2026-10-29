"""U3 render/display dimensions agree in both initializer paths, with native movies."""
from pathlib import Path
import hashlib,json,struct,sys
root=Path(__file__).resolve().parents[1]
patches=json.loads((root/'android/scripts/g2/recipes/bachata.json').read_text())['patches']
checked=0
for p in patches:
    if p['elf']!='big3-ps4_Shipping.elf' or not p['name'].startswith('Resolution:'):continue
    height=int(p['name'].split()[1][:-1]);width={720:1280,540:960,468:832,360:640}[height]
    writes={int(w['address'],16):w for w in p['writes']}
    assert len(writes)==len(p['writes'])
    spans=sorted((a,a+w['length']) for a,w in writes.items())
    assert all(a[1]<=b[0] for a,b in zip(spans,spans[1:]))
    pairs=([(0x902f7f,0x902f86),(0x902f94,0x902f9b),(0x902fa9,0x902fb0),(0x902fef,0x902ff6),
            (0x9035b6,0x9035bd),(0x9035cb,0x9035d2),(0x9035e0,0x9035e7),(0x903626,0x90362d)]
           if p['version']=='01.00' else
           [(0x12396b6,0x12396bd),(0x12396cb,0x12396d2),(0x12396e0,0x12396e7),(0x1239726,0x123972d)])
    for x,y in pairs:
        for a,expected,original in [(x,width,1920),(y,height,1080)]:
            w=writes[a];assert w['length']==4
            assert struct.unpack('<I',bytes.fromhex(w['bytes']))[0]==expected
            assert w['sha256']==hashlib.sha256(struct.pack('<I',original)).hexdigest()
    # Keep movie allocation at its encoded dimensions, independent of the scene.
    movie=[w for w in p['writes'] if w['length']>32]
    assert len(movie)==4
    for w in movie:
        b=bytes.fromhex(w['bytes']);assert struct.pack('<I',1920) in b or struct.pack('<I',960) in b
    if p['version']=='01.00':
        assert bytes.fromhex(writes[0x925124]['bytes'])==bytes.fromhex('41bd80070000b838040000909090')
    # Optional validation of EVERY write against the user's real executable.
    if len(sys.argv)>1 and p['version']=='01.00':
        sys.path.insert(0,str(root/'android/scripts/g2'))
        from validate_catalog import load_executable
        _,read=load_executable(Path(sys.argv[1]))
        for w in p['writes']:
            assert hashlib.sha256(read(int(w['address'],16),w['length'])).hexdigest()==w['sha256'],w
    checked+=1
assert checked==8
print('PASS: all 8 U3 resolution batches align scene/display dimensions and retain native movie surfaces; no overlapping writes')
