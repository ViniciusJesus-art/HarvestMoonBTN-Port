"""Build PNG, ICO and embedded SDL pixels from the supplied application logo."""
from pathlib import Path
import struct
import sys
import zlib

source=Path(sys.argv[1])
out=Path(sys.argv[2])
out.mkdir(parents=True,exist_ok=True)

def read_rgba_png(path):
 data=path.read_bytes()
 if data[:8]!=b'\x89PNG\r\n\x1a\n':
  raise ValueError(f'{path} is not a PNG image')
 offset=8
 compressed=bytearray()
 width=height=0
 while offset<len(data):
  length=struct.unpack_from('>I',data,offset)[0]
  tag=data[offset+4:offset+8]
  chunk=data[offset+8:offset+8+length]
  checksum=struct.unpack_from('>I',data,offset+8+length)[0]
  if zlib.crc32(tag+chunk)&0xffffffff!=checksum:
   raise ValueError(f'{path} has a corrupt PNG chunk')
  if tag==b'IHDR':
   width,height,depth,color,compression,filter_method,interlace=struct.unpack('>IIBBBBB',chunk)
   if depth!=8 or color!=6 or compression or filter_method or interlace:
    raise ValueError(f'{path} must be a non-interlaced 8-bit RGBA PNG')
  elif tag==b'IDAT':
   compressed.extend(chunk)
  elif tag==b'IEND':
   break
  offset+=length+12

 stride=width*4
 raw=zlib.decompress(compressed)
 if len(raw)!=height*(stride+1):
  raise ValueError(f'{path} has an invalid PNG image size')
 pixels=bytearray(height*stride)
 previous=bytearray(stride)
 offset=0
 for y in range(height):
  filter_type=raw[offset]
  offset+=1
  row=bytearray(raw[offset:offset+stride])
  offset+=stride
  for x in range(stride):
   left=row[x-4] if x>=4 else 0
   above=previous[x]
   upper_left=previous[x-4] if x>=4 else 0
   if filter_type==1:
    row[x]=(row[x]+left)&255
   elif filter_type==2:
    row[x]=(row[x]+above)&255
   elif filter_type==3:
    row[x]=(row[x]+(left+above)//2)&255
   elif filter_type==4:
    predictor=left+above-upper_left
    distances=(abs(predictor-left),abs(predictor-above),abs(predictor-upper_left))
    paeth=(left,above,upper_left)[distances.index(min(distances))]
    row[x]=(row[x]+paeth)&255
   elif filter_type!=0:
    raise ValueError(f'{path} uses an unsupported PNG filter')
  pixels[y*stride:(y+1)*stride]=row
  previous=row
 return width,height,pixels

width,height,source_pixels=read_rgba_png(source)
if width!=height or width<1:
 raise ValueError(f'{source} must be a non-empty square image')
(out/'hmbtn-native.png').write_bytes(source.read_bytes())

def pixels(size):
 return bytearray(b''.join(
  source_pixels[((y*height//size)*width+x*width//size)*4:
                ((y*height//size)*width+x*width//size)*4+4]
  for y in range(size) for x in range(size)
 ))

entries=[]
images=[]
offset=6+16*3
for size in (32,48,256):
 rgba=pixels(size)
 bgra=bytearray()
 for y in reversed(range(size)):
  for x in range(size):
   r,g,b,a=rgba[(y*size+x)*4:(y*size+x+1)*4]
   bgra.extend((b,g,r,a))
 mask=bytes(((size+31)//32)*4*size)
 image=struct.pack('<IiiHHIIiiII',40,size,size*2,1,32,0,len(bgra)+len(mask),0,0,0,0)+bgra+mask
 entries.append(struct.pack('<BBBBHHII',size%256,size%256,0,0,1,32,len(image),offset))
 images.append(image)
 offset+=len(image)
(out/'hmbtn-native.ico').write_bytes(struct.pack('<HHH',0,1,3)+b''.join(entries)+b''.join(images))

with (out/'app_icon.inc').open('w') as f:
 for name,content in [('app_icon_rgba',pixels(48)),('app_icon_png',source.read_bytes())]:
  f.write('static const unsigned char '+name+'[]={\n')
  for i in range(0,len(content),24):f.write(','.join(str(v) for v in content[i:i+24])+',\n')
  f.write('};\n')
