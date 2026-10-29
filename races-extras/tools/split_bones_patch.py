p = r'C:\CoA-Build\coa64\Engine\Source\Retail\RetailModel.cpp'
raw = open(p, 'rb').read().decode()
crlf = '\r\n' in raw
s = raw.replace('\r\n', '\n')
i = s.index('// CoA: the vertex constant file of the D3D9 backend holds 75')
end_marker = '    return ValidateSkin(b, m, error);\n}\n'
j = s.index(end_marker, i) + len(end_marker)
new = open(r'C:\CoA-Build\kv\split_bones.cpp.txt', encoding='utf-8').read()
s = s[:i] + new + s[j:]
open(p, 'wb').write((s.replace('\n', '\r\n') if crlf else s).encode())
print('patched')
