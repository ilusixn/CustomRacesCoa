import struct, io, sys
from PIL import Image
from quicktex import RawTexture
from quicktex.s3tc.bc3 import BC3Encoder
enc=BC3Encoder(18)
def write_blp_dxt5(im, path):
    im=im.convert('RGBA'); w,h=im.size; mips=[]
    while True:
        m=im.resize((w,h),Image.LANCZOS) if (w,h)!=im.size else im
        # pad tiny mips to 4x4 for block encode, encoder handles via block count
        pw,ph=max(4,w),max(4,h); mm=m if (pw,ph)==(w,h) else m.resize((pw,ph))
        data=enc.encode(RawTexture.frombytes(mm.tobytes(),pw,ph)).tobytes()
        mips.append(data)
        if w==1 and h==1: break
        w,h=max(1,w//2),max(1,h//2)
        if len(mips)==16: break
    hdr=struct.pack('<4sIBBBBII',b'BLP2',1,2,8,7,1,*im.size)
    offs=[];sizes=[];o=20+64+64+1024
    for d in mips: offs.append(o);sizes.append(len(d));o+=len(d)
    offs+= [0]*(16-len(offs)); sizes+=[0]*(16-len(sizes))
    out=hdr+struct.pack('<16I',*offs)+struct.pack('<16I',*sizes)+b'\0'*1024+b''.join(mips)
    open(path,'wb').write(out); return len(out)
if __name__=='__main__':
    import os; os.makedirs('blp',exist_ok=True)
    for n in sys.argv[1:]:
        sz=write_blp_dxt5(Image.open(f'Banshee{n}.png'),f'blp/BansheeSkin{n}.blp')
        back=Image.open(f'blp/BansheeSkin{n}.blp'); back.load(); print(n,sz,back.mode,back.size)
