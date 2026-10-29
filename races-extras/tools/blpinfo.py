import sys, struct
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\logo', r'C:\CoA-Build\eunoia', r'C:\CoA-Build\tools']
import mpqread, blp
m = mpqread.MPQ(r'C:\CoA-Build\kv\out\patch-ZZZE.MPQ')
for n in sorted(m.listfile()):
    if n.lower().endswith('.blp') and ('kyrian' in n.lower()):
        d = m.read(n)
        i = blp.read(d)
        print(n.split('\\')[-2] + '\\' + n.split('\\')[-1], len(d), 'enc', i['enc'], 'alpha', i['ad'], i['ae'], 'mips', i['mips'], i['w'], i['h'], 'mip0', i['size'][0], 'n', sum(1 for s in i['size'] if s))
