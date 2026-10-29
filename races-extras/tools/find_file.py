import sys,glob
sys.path[:0]=['C:/CoA-Build/tools','C:/CoA-Build/eunoia']
import mpqread
want=[a.lower() for a in sys.argv[1:]]
for f in sorted(glob.glob('*.MPQ'))+sorted(glob.glob('*/*.MPQ')):
    try: m=mpqread.MPQ(f); names=m.listfile()
    except Exception: continue
    for n in names or []:
        if n.lower() in want: print(f,n)
