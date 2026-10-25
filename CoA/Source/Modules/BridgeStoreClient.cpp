// AscBridgeStore.cpp:DbcInitDetour and AscHdDbc.cpp -- the DBC manager (FUN_101d3d20) run after the client's DBC
// initialisation.
//
//   HD model tables (0x101D9251..0x101D9BA3): when Data\Patch-Q.mpq opens and the command line does not say "-hd 0",
//   ten DBFilesClient\HD*.dbc files load (WDBC, the expected field count and row size; string columns point into the
//   file's string block, which is never freed), then each HD row overwrites the client row with its id, string
//   pointers included. The original's rows are the 32-bit records; here each row is read by the client record's own
//   reader, so the x64 record (pointers) is what gets written. CharacterFacialHairStyles has no id: its HD file
//   prepends one, and every client row whose (race, sex, variation) matches takes the HD row's five geosets.
#include "CoA/Source/Modules/BridgeStoreClient.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Runtime/Hd.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/CharHairGeosetsRec.h"
#include "WoW/Source/DB/Rec/CharSectionsRec.h"
#include "WoW/Source/DB/Rec/CharacterFacialHairStylesRec.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoExtraRec.h"
#include "WoW/Source/DB/Rec/CreatureDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/DB/Rec/EmotesTextSoundRec.h"
#include "WoW/Source/DB/Rec/HelmetGeosetVisDataRec.h"
#include "WoW/Source/DB/Rec/SpellVisualEffectNameRec.h"
#include "WoW/Source/DB/Rec/SpellVisualKitModelAttachRec.h"
#include <cstring>
#include <fstream>
#include <share.h>
#include <vector>

namespace {
    // FUN_101f3b50-style: the HD file's rows through T's reader; extraColumns leading id columns are skipped.
    template <class T> bool LoadHd(const char* path, int32_t extraColumns, std::vector<T>& rows) {
        SFileImpl* f;

        if (!SFile::OpenEx(nullptr, path, 0x20000, &f)) {
            return false;
        }

        uint32_t magic = 0;
        uint32_t count = 0;
        int32_t fields = 0;
        int32_t rowSize = 0;
        uint32_t stringSize = 0;
        bool ok = SFileReadFile(f, &magic, 4, nullptr, nullptr, 0) && magic == 0x43424457 && SFileReadFile(f, &count, 4, nullptr, nullptr, 0) && count != 0 &&
            SFileReadFile(f, &fields, 4, nullptr, nullptr, 0) && fields == T::s_numColumns + extraColumns && SFileReadFile(f, &rowSize, 4, nullptr, nullptr, 0) &&
            rowSize == T::s_rowSize + extraColumns * 4 && SFileReadFile(f, &stringSize, 4, nullptr, nullptr, 0);

        if (ok) {
            // Never freed: the client rows keep pointing into it.
            char* strings = static_cast<char*>(SMemAlloc(stringSize ? stringSize : 1, __FILE__, __LINE__, 0));
            rows.resize(count);

            for (T& row : rows) {
                for (int32_t i = 0; i < extraColumns; ++i) {
                    uint32_t skipped;
                    SFileReadFile(f, &skipped, 4, nullptr, nullptr, 0);
                }

                row.Read(f, strings);
            }

            ok = SFileReadFile(f, strings, stringSize, nullptr, nullptr, 0) != 0;
        }

        SFileCloseFile(f);

        if (!ok) {
            rows.clear();
        }

        return ok;
    }

    // FUN_1020cae0-style: the client row with the HD row's id is overwritten whole.
    template <class T> void ApplyHd(const char* path, WowClientDB<T>& db) {
        static_assert(!T::s_generatedID, "keyed by id");
        std::vector<T> rows;

        if (!LoadHd(path, 0, rows)) {
            return;
        }

        for (const T& row : rows) {
            if (T* dst = db.GetRecord(row.m_id)) {
                *dst = row;
            }
        }
    }

    // FUN_1020c940
    void ApplyFacialHair() {
        std::vector<CharacterFacialHairStylesRec> rows;

        if (!LoadHd("DBFilesClient\\HDCharacterFacialHairStyles.dbc", 1, rows)) {
            return;
        }

        for (const CharacterFacialHairStylesRec& src : rows) {
            for (int32_t i = 0; i < g_characterFacialHairStylesDB.m_numRecords; ++i) {
                CharacterFacialHairStylesRec& dst = g_characterFacialHairStylesDB.m_records[i];

                if (dst.m_raceID == src.m_raceID && dst.m_sexID == src.m_sexID && dst.m_variationID == src.m_variationID) {
                    memcpy(dst.m_geoset, src.m_geoset, sizeof(dst.m_geoset));
                }
            }
        }
    }

    // 0x101D9251: Data\Patch-Q.mpq opens (std::ifstream, _SH_DENYNO) and "-hd" is not "0". CoA64: and the Vulkan (DXVK)
    // renderer is active.
    bool HdEnabled() {
        if (!CoA::Hd::Available()) {
            return false;
        }

        {
            std::ifstream probe("Data\\Patch-Q.mpq", std::ios::in, _SH_DENYNO);

            if (!probe.is_open()) {
                return false;
            }
        }

        const char* hd = CoA::Script::CommandLineArg("hd");
        return !(hd && strcmp(hd, "0") == 0);
    }

    void ApplyHdTables() {
        if (!HdEnabled()) {
            return;
        }

        ApplyHd("DBFilesClient\\HDCreatureDisplayInfo.dbc", g_creatureDisplayInfoDB);
        ApplyHd("DBFilesClient\\HDCreatureDisplayInfoExtra.dbc", g_creatureDisplayInfoExtraDB);
        ApplyHd("DBFilesClient\\HDCreatureModelData.dbc", g_creatureModelDataDB);
        ApplyHd("DBFilesClient\\HDSpellVisualEffectName.dbc", g_spellVisualEffectNameDB);
        ApplyHd("DBFilesClient\\HDSpellVisualKitModelAttach.dbc", g_spellVisualKitModelAttachDB);
        ApplyFacialHair();
        ApplyHd("DBFilesClient\\HDCharHairGeosets.dbc", g_charHairGeosetsDB);
        ApplyHd("DBFilesClient\\HDCharSections.dbc", g_charSectionsDB);
        ApplyHd("DBFilesClient\\HDEmotesTextSound.dbc", g_emotesTextSoundDB);
        ApplyHd("DBFilesClient\\HDHelmetGeosetVisData.dbc", g_helmetGeosetVisDataDB);
    }
}

// The original loads all ten HD files before applying any; each table here applies right after its own load (the
// tables are independent).
void CoA::BridgeStoreClient::AfterClientDBInitialize() {
    CoA::BridgeStore::LoadTables();
    ApplyHdTables();
    CoA::BridgeStore::BuildIndexes();
}
