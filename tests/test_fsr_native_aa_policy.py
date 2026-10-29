"""Check the actual preset/visible control apply the same effective-AA rule."""
from pathlib import Path
import json,hashlib
root=Path(__file__).resolve().parents[1]
s=(root/'android/scripts/g2/game_app/Launcher.java').read_text()
make=s[s.index('private byte[] makePreset('):s.index('private static String hex(')]
assert 'name.equals("Disable anti-aliasing") ? UpscalePreset.disableNativeAa(' in make
assert 'if (!resolutionReady || !nativeAaReady) return null;' in make
assert 'if (group.getKey().equals("Disable anti-aliasing")) nativeAaReady = true;' in make
assert 'return fsrRequested?null:nativePreset;' in make
assert 'putBoolean("patch_Disable anti-aliasing"' not in s
assert 'state.setEnabled(!managedAa)' in s and 'state.setText(managedAa?"FSR"' in s
assert 'if(refreshNativeAa!=null)refreshNativeAa.run();' in s
patches=json.loads((root/'android/scripts/g2/recipes/bachata.json').read_text())['patches']
for elf in ['eboot.bin','big2-ps4_Shipping.elf','big3-ps4_Shipping.elf']:
 for version in ['01.00','01.02']:
  entries=[p for p in patches if p['elf']==elf and p['version']==version and p['name']=='Disable anti-aliasing']
  assert len(entries)==1,(elf,version)
  w,=entries[0]['writes'];patched=bytes.fromhex(w['bytes']);assert patched[-2:]==b'\xeb\x0d'
  original=patched[:-2]+b'\x74\x0d'
  assert hashlib.sha256(original).hexdigest()==w['sha256'],(elf,version)
print('PASS: automatic AA exclusion uses the six existing verified recipes, preserves saved preference and refuses incomplete/conflicting FSR batches')
