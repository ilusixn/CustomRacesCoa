import json, base64, struct, collections
base = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021\client\dbc-rows.json'
d = json.load(open(base))
for t, v in d.items():
    vals = collections.Counter()
    for r in v['rows']:
        for k, s in r['strings'].items():
            vals[(k, s)] += 1
    print(t, 'distinct strings', len(vals), list(vals.items())[:8])
cs = d['CharSections']['rows']
n = d['CharSections']['fields']
c = collections.Counter()
for r in cs:
    raw = base64 if False else None
    ints = struct.unpack('<10I', __import__('base64').b64decode(r['bytes']))
    c[(ints[1], ints[2], ints[3])] += 1
print(sorted(c.items()))
ch = d['CharBaseInfo']['rows']
print([__import__('base64').b64decode(r['bytes']).hex() for r in ch])
