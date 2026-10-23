"""Build an AppImage from an explicitly staged native runtime. Build tool only."""
import argparse, os, re, shutil, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('appdir',type=Path);p.add_argument('output',type=Path);p.add_argument('--tool',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);a=p.parse_args()
app=a.appdir.resolve();dest=app/'usr/lib';dest.mkdir(parents=True,exist_ok=True)
# SDL's backends are loaded dynamically; include their libraries plus ldd closure.
names=['libX11.so.6','libXext.so.6','libXcursor.so.1','libXi.so.6','libXfixes.so.3','libXrandr.so.2','libXss.so.1','libdbus-1.so.3','libasound.so.2','libpulse.so.0','libXrender.so.1','libXtst.so.6','libudev.so.1','libwayland-client.so.0','libwayland-cursor.so.0','libwayland-egl.so.1','libxkbcommon.so.0']
cache=subprocess.check_output(['ldconfig','-p'],text=True)
queue=[]
for name in names:
 match=re.search(r'^\s*'+re.escape(name)+r'\s+.*=> (.+)$',cache,re.M)
 if match:queue.append(Path(match[1]))
 # X11 and Pulse are the supported bundled defaults; Wayland/ALSA are optional.
 elif name in names[:9] or name=='libpulse.so.0':raise RuntimeError('Missing runtime library: '+name)
exclude={'libc.so.6','libm.so.6','libpthread.so.0','libdl.so.2','librt.so.1','libresolv.so.2','ld-linux-x86-64.so.2'}
seen=set();licenses=app/'usr/share/licenses/runtime-libraries';licenses.mkdir(parents=True,exist_ok=True)
while queue:
 lib=queue.pop();name=lib.name
 if name in seen or name in exclude:continue
 seen.add(name);shutil.copy2(lib,dest/name)
 result=subprocess.run(['ldd',str(lib)],capture_output=True,text=True,check=True)
 if 'not found' in result.stdout:raise RuntimeError(result.stdout)
 queue.extend(Path(s) for s in re.findall(r'=> (/\S+)',result.stdout))
 # Include the redistribution notices of each bundled distribution library.
 query=subprocess.run(['dpkg-query','-S',str(lib)],capture_output=True,text=True)
 if query.returncode and str(lib).startswith('/lib/'):
  query=subprocess.run(['dpkg-query','-S','/usr'+str(lib)],capture_output=True,text=True)
 if query.returncode:raise RuntimeError('Cannot locate library copyright: '+str(lib))
 package=query.stdout.split(': ')[0].split(':')[0]
 copyright=Path('/usr/share/doc')/package/'copyright'
 if not copyright.is_file():raise RuntimeError('Missing copyright: '+str(copyright))
 shutil.copy2(copyright,licenses/(package+'.txt'))
root=Path(__file__).resolve().parent
shutil.copy2(root/'AppRun',app/'AppRun');(app/'AppRun').chmod(0o755)
shutil.copy2(root/'hmbtn-native.desktop',app/'hmbtn-native.desktop')
shutil.copy2(app/'usr/share/icons/hicolor/256x256/apps/hmbtn-native.png',app/'hmbtn-native.png')
shutil.copy2(app/'hmbtn-native.png',app/'.DirIcon')
a.output.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([str(a.tool.resolve()),'--appimage-extract-and-run','--runtime-file',str(a.runtime.resolve()),'--no-appstream',str(app),str(a.output.resolve())],check=True,env=dict(os.environ,ARCH='x86_64',SOURCE_DATE_EPOCH=os.environ.get('SOURCE_DATE_EPOCH','0')))
print('AppImage:',a.output)
