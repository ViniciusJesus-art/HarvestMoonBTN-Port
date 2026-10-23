"""Refuse game resources, PS1 build outputs, SDKs or source in runtime staging."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
for path in root.rglob('*'):
 rel=path.relative_to(root)
 if set(part.lower() for part in rel.parts)&{'sdk','src','disc','data','a_file','tools'}:
  raise SystemExit('Unexpected packaged directory: '+str(rel))
 if path.is_file() and path.suffix.lower() in {'.bin','.xa','.stm','.tex','.hdt','.cue','.iso','.py','.c','.h'}:
  raise SystemExit('Unexpected packaged resource/source: '+str(rel))
assert any(p.name in {'hmbtn_native','hmbtn_native.exe'} for p in root.rglob('*'))
print('Runtime payload contains no disc data, MIPS overlays, SDK or source.')
