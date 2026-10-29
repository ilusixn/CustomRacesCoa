import sys,struct
sys.path[:0]=['C:/CoA-Build/wow-retroport-skill/wow-retroport/scripts/bundle/vendor']
from wotlkconv.m2 import parse_m2
from wotlkconv.m2.types import VALUE_FORMATS
import os
mp, folder, stem = sys.argv[1:4]
m = parse_m2(open(mp,'rb').read(),'x')
tot_bad = 0
for i, s in enumerate(m.sequences):
    if s['flags'] & 0x20: continue
    p = os.path.join(folder, '%s%04d-%02d.anim' % (stem, s['id'], s['variation_index']))
    if not os.path.exists(p): print('missing', s['id']); continue
    d = open(p,'rb').read(); bad = 0
    for t in m.tracks():
        if t.global_sequence >= 0 or i >= len(t.timestamp_spans): continue
        n, o = t.timestamp_spans[i]
        if not n: continue
        fmt = VALUE_FORMATS[t.kind] if hasattr(t,'values') else None
        st = struct.calcsize('<'+fmt) if fmt else 0
        vn, vo = t.value_spans[i] if hasattr(t,'value_spans') else (0,0)
        if o + n*4 > len(d) or (fmt and vo + vn*st > len(d)): bad += 1
    tot_bad += bad
print(mp, 'sequences', len(m.sequences), 'bad tracks total', tot_bad)
