import os as _os; COA_ROOT = _os.environ.get("COA_ROOT", ".")  # [release] server tree root
# -*- coding: utf-8 -*-
"""Le arquivos de dentro dos MPQ usando a libstorm que ja temos."""
import ctypes, sys, os
lib = ctypes.CDLL(_os.path.join(_os.environ.get('MPQ_TOOLS', 'client/build'), 'libstorm.so'))
lib.SFileOpenArchive.argtypes=[ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
lib.SFileOpenFileEx.argtypes=[ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
lib.SFileGetFileSize.argtypes=[ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint)]
lib.SFileReadFile.argtypes=[ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_uint), ctypes.c_void_p]
lib.SFileCloseFile.argtypes=[ctypes.c_void_p]
lib.SFileCloseArchive.argtypes=[ctypes.c_void_p]

def abre(mpq):
    h=ctypes.c_void_p()
    if not lib.SFileOpenArchive(mpq.encode(),0,0x00000100,ctypes.byref(h)):
        return None
    return h

def le(h, nome):
    f=ctypes.c_void_p()
    if not lib.SFileOpenFileEx(h, nome.encode(), 0, ctypes.byref(f)): return None
    alto=ctypes.c_uint(0)
    tam=lib.SFileGetFileSize(f, ctypes.byref(alto))
    buf=ctypes.create_string_buffer(tam)
    lido=ctypes.c_uint(0)
    lib.SFileReadFile(f, buf, tam, ctypes.byref(lido), None)
    lib.SFileCloseFile(f)
    return buf.raw[:lido.value]

def listfile(h):
    d=le(h,'(listfile)')
    if not d: return []
    return d.decode('utf-8','replace').replace('\r\n','\n').split('\n')

if __name__=='__main__':
    acao=sys.argv[1]
    if acao=='ls':
        h=abre(sys.argv[2]); 
        if not h: print("nao abriu"); sys.exit(1)
        pad=sys.argv[3] if len(sys.argv)>3 else ''
        for l in listfile(h):
            if pad.lower() in l.lower(): print(l)
        lib.SFileCloseArchive(h)
    elif acao=='cat':
        h=abre(sys.argv[2])
        d=le(h,sys.argv[3])
        if d is None: print("nao achou", sys.argv[3]); sys.exit(1)
        sys.stdout.buffer.write(d)
        lib.SFileCloseArchive(h)
