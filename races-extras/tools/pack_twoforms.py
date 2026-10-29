# Pack out/patch/** into patch-ZZY.MPQ (same StormLib flags as pack_race.py) and verify every file reads back.
#   python pack_zzy.py [archive name]
import os, sys, ctypes
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, r'C:\CoA-Build\eunoia', r'C:\CoA-Build\tools']
import storm, mpqread

S = storm.S
B = chr(92)
name = sys.argv[1] if len(sys.argv) > 1 else 'patch-ZZZZD.MPQ'
root = 'C:/CoA-Build/kv/out/twoforms_patch'
files = []
for d, _, names in os.walk(root):
    for n in names:
        disk = os.path.join(d, n)
        files.append((os.path.relpath(disk, root).replace('/', B), disk))
S.SFileCreateArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p)]
S.SFileCreateArchive.restype = ctypes.c_bool
S.SFileAddFileEx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
S.SFileAddFileEx.restype = ctypes.c_bool
arc = os.path.join(r'C:\CoA-Build\logo', name)
if os.path.exists(arc):
    os.remove(arc)
h = ctypes.c_void_p()
assert S.SFileCreateArchive(arc.encode(), 0x01000000 | 0x00100000 | 0x00200000, len(files) + 64, ctypes.byref(h))
for rel, disk in files:
    assert S.SFileAddFileEx(h, os.path.abspath(disk).encode(), rel.encode(), 0x200 | 0x80000000, 2, 2), rel
S.SFileCloseArchive(h)
m = mpqread.MPQ(arc)
for rel, disk in files:
    assert m.read(rel) == open(disk, 'rb').read(), rel
print('built and verified', arc, len(files), 'files', os.path.getsize(arc), 'bytes')
for rel, _ in files:
    print('  ', rel)
