#include "CoA/Source/Runtime/Dbc.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "WoW/Source/Client.h"

namespace {
    // SFile::OpenEx (0x424B50) with the DLL's flags, SFileReadFile (0x422530), SFileCloseFile (0x422910).
    bool ReadFromArchives(const char* path, std::vector<uint8_t>& out) {
        SFileImpl* file = nullptr;

        if (!SFile::OpenEx(nullptr, path, 0x20000, &file) || !file) {
            return false;
        }

        uint32_t size = SFileGetFileSize(file, nullptr);
        out.resize(size);
        bool ok = size == 0 || SFileReadFile(file, out.data(), size, nullptr, nullptr, 0) != 0;
        SFileCloseFile(file);
        return ok;
    }

    // FUN_101b0c80: which of the 16 locale slots this client reads (s_locale, jump table 0x101b0d38).
    uint32_t LocaleSlot() {
        static const uint32_t map[] = { 0, 2, 3, 4, 6, 8, 9, 10, 11 };
        uint32_t locale = static_cast<uint32_t>(s_locale);
        return locale <= 8 ? map[locale] : 15;
    }
}

void CoA::Dbc::InstallClient() {
    CoA::Dbc::SetFileReader(ReadFromArchives);
    CoA::Dbc::SetLocaleSlot(LocaleSlot);
}
