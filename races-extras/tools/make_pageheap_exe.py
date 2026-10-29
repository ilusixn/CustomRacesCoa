"""Copy CoA64.exe to CoA64_PH.exe with FLG_HEAP_PAGE_ALLOCS set through the PE load-config GlobalFlagsSet (no registry / admin needed)."""
import struct, shutil, sys

src = r'C:\CoA-Build\coa64\bin\Win\CoA64.exe'
dst = r'C:\Ascension Local\CoA64_PH.exe'
shutil.copyfile(src, dst)
d = bytearray(open(dst, 'rb').read())
pe = struct.unpack_from('<I', d, 0x3c)[0]
assert d[pe:pe + 4] == b'PE\0\0'
nsec = struct.unpack_from('<H', d, pe + 6)[0]
optsize = struct.unpack_from('<H', d, pe + 20)[0]
opt = pe + 24
magic = struct.unpack_from('<H', d, opt)[0]
assert magic == 0x20b, hex(magic)
lc_rva, lc_size = struct.unpack_from('<II', d, opt + 112 + 10 * 8)   # data directory 10 = load config
print('load config rva', hex(lc_rva), 'size', lc_size)
assert lc_rva and lc_size >= 0x20
sec = opt + optsize
off = None
for i in range(nsec):
    s = sec + i * 40
    vsize, va, rsize, raw = struct.unpack_from('<IIII', d, s + 8)
    if va <= lc_rva < va + max(vsize, rsize):
        off = raw + lc_rva - va
assert off
size_field = struct.unpack_from('<I', d, off)[0]
print('struct size', size_field, 'old set', hex(struct.unpack_from('<I', d, off + 8)[0]), hex(struct.unpack_from('<I', d, off + 12)[0]))
# 64-bit layout: Size(4) Time(4) Major(2) Minor(2) GlobalFlagsClear(4) GlobalFlagsSet(4)
struct.pack_into('<I', d, off + 12, struct.unpack_from('<I', d, off + 12)[0] | 0x02000000)
open(dst, 'wb').write(d)
print('wrote', dst)
