import sys
sys.path[:0] = [r'C:\CoA-Build\kv', r'C:\CoA-Build\logo', r'C:\CoA-Build\esteria_racials']
import effread, blp
for n in ('Interface/Icons/CoA_Race_Saberon.blp', 'Interface/Icons/CoA_Race_LightforgedDraenei.blp'):
    d, p = effread.read(n)
    i = blp.read(d)
    print(n, p.split(chr(92))[-1], len(d), i['enc'], i['w'], i['h'], i['ad'], i['ae'], i['mips'])
