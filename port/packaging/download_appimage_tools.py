"""Fetch pinned official build tools, verifying SHA-256 (never used by end users)."""
import hashlib, sys, urllib.request
from pathlib import Path
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
files=[
 ('appimagetool.AppImage','https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage','ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0'),
 ('runtime-x86_64','https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64','2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d')]
for name,url,expected in files:
 path=out/name
 if not path.exists():
  tmp=path.with_suffix('.download')
  try:
   with urllib.request.urlopen(url,timeout=90) as src,tmp.open('wb') as dst:
    while chunk:=src.read(1024*1024):dst.write(chunk)
   if hashlib.sha256(tmp.read_bytes()).hexdigest()!=expected:raise RuntimeError('Checksum mismatch: '+name)
   tmp.replace(path)
  finally:tmp.unlink(missing_ok=True)
 if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise RuntimeError('Checksum mismatch: '+name)
 path.chmod(0o755)
 print(name,expected)
