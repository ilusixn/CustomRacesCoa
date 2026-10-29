"""Hand port of the Retail hooks of Esteria's SFile2-Core.cpp into CoA64 (no extension manager, no embedded files)."""
P = r'C:\CoA-Build\coa64\Engine\Source\BlizzardCore\SFile2-Core.cpp'
raw = open(P, 'rb').read().decode('utf-8')
crlf = raw.count('\r\n') > raw.count('\n') / 2
s = raw.replace('\r\n', '\n')
NL = chr(10)
BS = chr(92)


def once(old, new):
    global s
    assert old in s, old[:80]
    s = s.replace(old, new, 1)


def ret(body):
    return '#if defined(WOWDECOMP_RETAIL_ASSETS)' + NL + body + '#endif' + NL


once('#include "Engine/Source/Storm/Source/MD5.h"' + NL, '#include "Engine/Source/Storm/Source/MD5.h"' + NL + ret('#include "Engine/Source/Retail/CascSource.h"' + NL))
once('bool STORMAPI SFile::FileGetIsLocalAmount(const char* name, int64_t& local, int64_t& total) {' + NL,
     'bool STORMAPI SFile::FileGetIsLocalAmount(const char* name, int64_t& local, int64_t& total) {' + NL + ret(
         '    if (Retail::SourceForPath(name).Handles(name)) {' + NL + '        uint32_t size = 0;' + NL +
         '        bool cached = Retail::SourceForPath(name).CachedOriginalSize(name, size);' + NL +
         '        local = total = size;' + NL + '        return cached;' + NL + '    }' + NL))
once('int32_t STORMAPI SFile::FileExistsInArchives(const char* name) {' + NL,
     'int32_t STORMAPI SFile::FileExistsInArchives(const char* name) {' + NL + ret(
         '    if (Retail::SourceForPath(name).Handles(name)) {' + NL + '        std::string error;' + NL +
         '        return Retail::SourceForPath(name).Exists(name, error);' + NL + '    }' + NL))
sig = 'int32_t STORMAPI SFileLoadFileEx(SArchive* archive, const char* name, void** data, uint32_t* size, uint32_t extra, uint32_t flags, SOVERLAPPED* overlapped) {' + NL
once(sig, sig + ret('    if (!archive && Retail::SourceForPath(name).Handles(name)) {' + NL +
                    '        return SFile::ReadFile(nullptr, name, data, size, extra, flags, overlapped);' + NL + '    }' + NL))
sig = 'bool System_SFile2::FindFile(const char* name, char* path, uint32_t size, uint32_t flags, SFILE_TYPE* type, System_SFile::ARCHIVEREC** archive) {' + NL
once(sig, sig + ret('    if (Retail::SourceForPath(name).Handles(name)) {' + NL + '        if (archive) *archive = nullptr;' + NL +
                    '        std::string error;' + NL + '        if (!Retail::SourceForPath(name).Exists(name, error)) return false;' + NL +
                    '        Blizzard::String::Copy(path, name, size);' + NL + '        *type = SFILE_RETAIL;' + NL + '        return true;' + NL + '    }' + NL))
once('        case SFILE_MEMORY: {' + NL + '            uint32_t available', '        case SFILE_MEMORY:' + NL + '        case SFILE_RETAIL: {' + NL + '            uint32_t available')
a = s.index('uint32_t STORMAPI SFileGetFileSize(')
b = s.index('        case SFILE_MEMORY: {', a)
s = s[:b] + '        case SFILE_MEMORY:' + NL + '        case SFILE_RETAIL: {' + s[b + len('        case SFILE_MEMORY: {'):]
a = s.index('int32_t STORMAPI SFile::SetFilePointer(')
b = s.index('        case SFILE_MEMORY: {', a)
seek = '''        // Not in the original: Retail files are verified and prepared in memory at open.
        case SFILE_RETAIL: {
            int64_t move = distanceHigh ? (int64_t(*distanceHigh) * 0x100000000LL + uint32_t(distance)) : int64_t(distance);
            int64_t origin = method == 0 ? 0 : method == 1 ? file->m_position : file->m_size;
            if (method > 2 || move < -origin || move > int64_t(UINT32_MAX) - origin) {
                SErrSetLastError(87);
                result = -1;
            } else {
                file->m_position = static_cast<uint32_t>(origin + move);
                if (distanceHigh) *distanceHigh = 0;
                result = static_cast<int32_t>(file->m_position);
            }
            break;
        }

'''
s = s[:b] + seek + s[b:]
openblock = '''    if (!archive && Retail::SourceForPath(name).Handles(name)) {
        *result = nullptr;
        std::vector<uint8_t> bytes;
        std::string error;
        if (!Retail::SourceForPath(name).Read(name, bytes, error)) {
            Blizzard::Log::Printf(1, "Logs/RetailAssets.log", "%s: %s\\n", name, error.c_str());
            SErrSetLastError(2);
            return 0;
        }
        SFileImpl* file = new (0u, __FILE__, __LINE__) SFileImpl(SFILE_RETAIL);
        file->m_size = static_cast<uint32_t>(bytes.size());
        file->m_position = 0;
        file->m_name = Blizzard::String::Duplicate(name);
        file->m_data = static_cast<uint8_t*>(Blizzard::Memory::Allocate(std::max(size_t(1), bytes.size()), 0, __FILE__, __LINE__));
        if (!file->m_data) {
            delete file;
            SErrSetLastError(8);
            return 0;
        }
        if (!bytes.empty()) std::memcpy(file->m_data, bytes.data(), bytes.size());
        *result = file;
        return 1;
    }
'''
anchor = '    *result = nullptr;' + NL + NL + '    System_SFile::ARCHIVEREC* handle = nullptr;'
once(anchor, ret(openblock) + NL + anchor)
open(P, 'wb').write((s.replace(NL, '\r\n') if crlf else s).encode('utf-8'))
print('ok')
