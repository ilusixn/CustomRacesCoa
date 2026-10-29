import sys,glob
sys.path[:0]=['C:/CoA-Build/tools','C:/CoA-Build/eunoia']
import mpqread
B=chr(92)
seen={}
for f in sorted(glob.glob('*.MPQ'))+sorted(glob.glob('*/*.MPQ')):
    try: m=mpqread.MPQ(f); names=m.listfile()
    except Exception: continue
    for n in names or []:
        l=n.lower()
        if l.startswith('interface'+B+'glues'+B+'models'+B+'ui_') and l.endswith('.m2'):
            seen.setdefault(n,[]).append(f)
for n,fs in sorted(seen.items()): print(n,fs[-1])
