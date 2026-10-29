import sys, os
sys.path[:0] = [r'C:\CoA-Build\esteria_racials']
import estlib
BS = chr(92)
C = estlib.Client(estlib.OUR_DATA)
# estlib orders four-letter suffixes wrongly: the game loads ZZZ < ZZZA < ZZZC < ZZZD < ZZZE ... (alphabetical)
TOP = [os.path.join(estlib.OUR_DATA, 'patch-ZZZ%s.MPQ' % s) for s in 'ZYXWVUTSRQPONMLKJIHGFEDCBA']  # highest first


def read(name):
    for p in TOP:
        if os.path.exists(p) and not p.endswith('ZZZE.MPQ'):   # ZZZE is our own Kyrian/Venthyr archive: always rebuild from the base
            m = C.mpq(p)
            if m is not None and m.has(name):
                return m.read(name), p
    return C.read(name)
