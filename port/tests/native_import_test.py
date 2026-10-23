"""Synthetic disc only: native import fidelity, exclusion, rollback and Unicode paths."""
from pathlib import Path
import hashlib, json, struct, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import afile, psx_disc
EXE=str(Path(sys.argv[1]).resolve())
def run(image,out,success):
 p=subprocess.run([EXE,'--import-bin',str(image),'--data',str(out)],capture_output=True,text=True,timeout=20)
 assert (p.returncode==0)==success,(p.returncode,p.stdout,p.stderr)
 return p
with tempfile.TemporaryDirectory(prefix='hmbtn-native-import-') as temp:
 root=Path(temp);files=root/'source';files.mkdir();(files/'STR').mkdir()
 layout=psx_disc.load_layout(ROOT/'disc/disc.xml')
 members=afile.load_members(ROOT/'disc/a_file.txt')
 chunks=[bytes([i+1])*2048 for i in range(32)]
 (files/'A_FILE.BIN').write_bytes(b''.join(chunks))
 (files/'A_FILE.HDT').write_bytes(struct.pack('<33I',*[i*2048 for i in range(33)]))
 for name in ('SYSTEM.CNF','SLUS_011.15','COPYRIGH.TXT','BIBLIOGP.TXT','ABSTRACT.TXT','DUMMY.DAT'):
  (files/name).write_bytes(b'BOOT = cdrom:\\SLUS_011.15;1\r\n' if name=='SYSTEM.CNF' else b'synthetic')
 (files/'license_data.dat').write_bytes(bytes(12*2336))
 for i in range(1,5):(files/'STR'/f'{i:02}.XA').write_bytes(bytes([i])*2336)
 image=root/'disco com espaços e acentos ã.bin'
 psx_disc.build(ROOT/'disc/disc.xml',files,{},image,None)
 out=root/'recursos válidos'/'data';run(image,out,True)
 manifest=json.loads((out/'native-import.json').read_text(encoding='utf-8'))
 expected={'A_FILE/'+m.file:chunks[m.index] for m in members if m.source=='original'}
 expected.update({f'disc/STR/{i:02}.XA':bytes([i])*2336 for i in range(1,5)})
 assert manifest['resources_only'] and set(manifest['files'])==set(expected)
 assert manifest['source']==str(image)
 assert {str(p.relative_to(out)).replace('\\','/') for p in out.rglob('*') if p.is_file()}==set(expected)|{'native-import.json'}
 for name,data in expected.items():
  assert (out/name).read_bytes()==data,name
  assert manifest['files'][name]=={'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}
 protected=out/'keep.txt';protected.write_text('must survive')
 run(image,out,False);assert protected.read_text()=='must survive'
 truncated=root/'truncated.bin';truncated.write_bytes(image.read_bytes()[:-1])
 run(truncated,root/'bad-length',False);assert not (root/'bad-length').exists()
 # Break a resource's raw sector after early files were already written.
 damaged=bytearray(image.read_bytes());placement=psx_disc.place(layout)
 archive=next(e for e in psx_disc._entries(layout.root) if e.path=='A_FILE.BIN')
 damaged[(archive.lba+9)*2352+15]=1
 broken=root/'broken.bin';broken.write_bytes(damaged)
 failed=root/'rollback';run(broken,failed,False)
 assert not failed.exists() and not list(root.glob('rollback.import-*'))
 # Bad HDT: out of range / unaligned; must reject before writing resources.
 (files/'A_FILE.HDT').write_bytes(struct.pack('<33I',0,1,*[i*2048 for i in range(2,33)]))
 bad=root/'bad-offset.bin';psx_disc.build(ROOT/'disc/disc.xml',files,{},bad,None)
 run(bad,root/'bad-offset',False);assert not (root/'bad-offset').exists()
print('Native resource import, hashes, XA, no PS1 code, Unicode, refusal to overwrite and rollback passed.')
