#!/usr/bin/env python3
"""Row-level delta for WDBC files, so we never ship Blizzard/Ascension DBC files.

  dbcdelta.py make  <original.dbc> <modified.dbc> <out.json>
  dbcdelta.py apply <original.dbc> <delta.json>   <out.dbc>
  dbcdelta.py check <original.dbc> <delta.json>   <modified.dbc>   (exit 1 if apply(original) != modified)

Rows are keyed by their first column (ID). Columns whose every value in the modified file is a valid
string-table offset (or 0) and that resolve to readable text are treated as string columns and stored
as text; everything else is stored as raw 32-bit integers. Files whose record size is not a multiple
of 4 (e.g. CharBaseInfo, 2 bytes per row) are compared as whole records (hex)."""
import json, struct, sys

def load(path):
    b = open(path, "rb").read()
    magic, n, f, rs, ss = struct.unpack("<4sIIII", b[:20])
    assert magic == b"WDBC", path
    recs = [b[20 + i * rs:20 + (i + 1) * rs] for i in range(n)]
    return dict(f=f, rs=rs, recs=recs, strs=b[20 + n * rs:20 + n * rs + ss])

def text(strs, o):
    return strs[o:strs.find(b"\0", o)].decode("utf8", "surrogateescape")

def string_cols(d):
    if d["rs"] % 4: return set()
    nc, strs, cols = d["rs"] // 4, d["strs"], set()
    rows = [struct.unpack("<%dI" % nc, r) for r in d["recs"]]
    for c in range(1, nc):
        vals = [r[c] for r in rows]
        if not any(vals): continue
        if all(v == 0 or (v < len(strs) and strs[v - 1] == 0) for v in vals):
            sample = [text(strs, v) for v in vals if v][:50]
            if sum(1 for s in sample if len(s) >= 3 and s.isprintable()) >= max(1, len(sample) // 2):
                cols.add(c)
    return cols

def norm(d, cols):
    """record -> comparable tuple (strings resolved)."""
    out = {}
    for r in d["recs"]:
        if d["rs"] % 4:
            out[r.hex()] = r.hex(); continue
        v = list(struct.unpack("<%dI" % (d["rs"] // 4), r))
        for c in cols: v[c] = {"s": text(d["strs"], v[c])} if v[c] else 0
        out[v[0]] = v
    return out

def make(orig, mod, out):
    a, b = load(orig), load(mod)
    assert (a["f"], a["rs"]) == (b["f"], b["rs"]), "layout differs"
    cols = string_cols(b) | string_cols(a)
    na, nb = norm(a, cols), norm(b, cols)
    if a["rs"] % 4:
        delta = dict(bytes=True, removed=sorted(set(na) - set(nb)), added=sorted(set(nb) - set(na)))
    else:
        delta = dict(bytes=False, string_cols=sorted(cols), removed=sorted(k for k in na if k not in nb),
                     upsert=[nb[k] for k in nb if na.get(k) != nb[k]])
    delta.update(fields=a["f"], record_size=a["rs"], base_rows=len(a["recs"]), order=list(nb))
    json.dump(delta, open(out, "w"), ensure_ascii=False, separators=(",", ":"))
    n = len(delta.get("upsert", delta.get("added", [])))
    print(f"{out}: {n} rows added/changed, {len(delta['removed'])} removed")

def apply(orig, delta_path, out):
    a, d = load(orig), json.load(open(delta_path))
    strs = bytearray(a["strs"]); index = {}
    def addstr(s):
        if s not in index:
            index[s] = len(strs); strs.extend(s.encode("utf8", "surrogateescape") + b"\0")
        return index[s]
    if d["bytes"]:
        rows = {r.hex(): r for r in a["recs"]}
        for k in d["removed"]: rows.pop(k, None)
        for k in d["added"]: rows[k] = bytes.fromhex(k)
        recs = [rows[k] for k in d["order"]]
    else:
        nc = a["rs"] // 4
        rows = {struct.unpack_from("<I", r)[0]: r for r in a["recs"]}
        for k in d["removed"]: rows.pop(k, None)
        for v in d["upsert"]:
            v = [addstr(x["s"]) if isinstance(x, dict) else x for x in v]
            rows[v[0]] = struct.pack("<%dI" % nc, *v)
        recs = [rows[k] for k in d["order"]]
    open(out, "wb").write(struct.pack("<4sIIII", b"WDBC", len(recs), a["f"], a["rs"], len(strs)) + b"".join(recs) + bytes(strs))

def check(orig, delta_path, mod):
    import os, tempfile
    tmp = tempfile.mktemp(suffix=".dbc"); apply(orig, delta_path, tmp)
    x, y = load(tmp), load(mod); os.unlink(tmp)
    cols = string_cols(y) | set(json.load(open(delta_path)).get("string_cols", []))
    ok = list(norm(x, cols).items()) == list(norm(y, cols).items())
    print(("OK " if ok else "DIFFERS ") + delta_path); sys.exit(0 if ok else 1)

if __name__ == "__main__":
    {"make": make, "apply": apply, "check": check}[sys.argv[1]](*sys.argv[2:5])
