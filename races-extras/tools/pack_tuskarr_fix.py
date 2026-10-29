"""patch-ZZZZE.MPQ: the Tuskarr male model whose tracks match the .anim files (the sheath rebuild in patch-ZZZE has 22 tracks per animation past the end of them)."""
import os, sys, ctypes
sys.path[:0] = [r'C:\CoA-Build\slime', r'C:\CoA-Build\eunoia', r'C:\CoA-Build\tools']
import storm, mpqread
S = storm.S
B = chr(92)
rel = B.join(['Character', 'Tuskarr', 'Male', 'TuskarrMale.m2'])
disk = r'C:\CoA-Build\slime\out\sheath_work\TuskarrMale\TuskarrMale.m2'
S.SFileCreateArchive.argtypes = [ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p)]
S.SFileCreateArchive.restype = ctypes.c_bool
S.SFileAddFileEx.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
S.SFileAddFileEx.restype = ctypes.c_bool
arc = r'C:\CoA-Build\logo\patch-ZZZZE.MPQ'
if os.path.exists(arc):
    os.remove(arc)
h = ctypes.c_void_p()
assert S.SFileCreateArchive(arc.encode(), 0x01000000 | 0x00100000 | 0x00200000, 64, ctypes.byref(h))
assert S.SFileAddFileEx(h, disk.encode(), rel.encode(), 0x200 | 0x80000000, 2, 2)
S.SFileCloseArchive(h)
assert mpqread.MPQ(arc).read(rel) == open(disk, 'rb').read()
print('built', arc, os.path.getsize(arc))
