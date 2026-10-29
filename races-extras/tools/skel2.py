import sys, io
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\logo', r'C:\CoA-Build\esteria_racials']
import effread, blp
from PIL import Image
for n in ('Character/Skeleton/Male/SkeletonMaleSkin00_00.blp', 'Character/Skeleton/Male/SkeletonMaleFaceLower00_00.blp',
          'Character/Skeleton/Male/SkeletonMaleFaceUpper00_00.blp', 'Character/Skeleton/Female/SkeletonFemaleSkin00_00.blp'):
    d, p = effread.read(n)
    if not d:
        print(n, 'missing')
        continue
    i = blp.read(d)
    print(n, p.split(chr(92))[-1], i['enc'], i['w'], i['h'])
    try:
        Image.open(io.BytesIO(d)).convert('RGBA').save(r'C:\CoA-Build\kv\sk_' + n.split('/')[-1] + '.png')
    except Exception as e:
        print('  ', e)
