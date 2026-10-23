"""Original farm/moon application mark; generated without game/disc artwork.
Build-time only, standard library. Outputs PNG, ICO and embedded SDL pixels.
"""
from pathlib import Path
import struct, sys, zlib
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def pixels(n):
 data=bytearray()
 for y in range(n):
  for x in range(n):
   a,b=x*256/n,y*256/n;c=(24,61,51,255)
   if (a-193)**2+(b-56)**2<26**2:c=(253,221,129,255)
   if (a-204)**2+(b-46)**2<23**2:c=(24,61,51,255)
   if b>171:c=(80,132,60,255)
   if b>171 and int((a+b*.8)/26)%2:c=(106,155,65,255)
   if 53<a<183 and 115<b<199:c=(181,92,49,255)
   if 40<a<196 and 70+abs(a-118)*.68<b<126:c=(231,185,100,255)
   if 104<a<137 and 144<b<199:c=(42,62,45,255)
   if 66<a<88 and 141<b<163:c=(249,226,161,255)
   if 152<a<174 and 141<b<163:c=(249,226,161,255)
   if (a-38)**2+(b-157)**2<22**2:c=(43,107,63,255)
   if a<9 or b<9 or a>247 or b>247:c=(239,207,139,255)
   data.extend(c)
 return data
def png(n,data):
 def chunk(tag,p):return struct.pack('>I',len(p))+tag+p+struct.pack('>I',zlib.crc32(tag+p))
 scan=b''.join(b'\0'+data[y*n*4:(y+1)*n*4] for y in range(n))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',n,n,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
large=png(256,pixels(256));(out/'hmbtn-native.png').write_bytes(large)
entries=[];images=[];offset=6+16*3
for n in (32,48,256):
 p=pixels(n);bgra=bytearray()
 for y in reversed(range(n)):
  for x in range(n):
   r,g,b,a=p[(y*n+x)*4:(y*n+x+1)*4];bgra.extend((b,g,r,a))
 mask=bytes(((n+31)//32)*4*n)
 image=struct.pack('<IiiHHIIiiII',40,n,n*2,1,32,0,len(bgra)+len(mask),0,0,0,0)+bgra+mask
 entries.append(struct.pack('<BBBBHHII',n%256,n%256,0,0,1,32,len(image),offset));images.append(image);offset+=len(image)
(out/'hmbtn-native.ico').write_bytes(struct.pack('<HHH',0,1,3)+b''.join(entries)+b''.join(images))
with (out/'app_icon.inc').open('w') as f:
 for name,content in [('app_icon_rgba',pixels(48)),('app_icon_png',large)]:
  f.write('static const unsigned char '+name+'[]={\n')
  for i in range(0,len(content),24):f.write(','.join(str(v) for v in content[i:i+24])+',\n')
  f.write('};\n')
