// AscClientDbcPatch.cpp's handlers: each patches one of the client's WowClientDB tables. The wire record is read item
// by item (ClientDbcPatch::ReadValues), the row with that id is overwritten in place (only the fields the handler
// writes), or a new zeroed row is allocated, written and inserted into the table's index (ClientDbcPatch::Insert). The
// per-table writers come from ClientDbcPatch.generated.inc (Tools/coa/gen_client_dbc_patch.py): the original's 32-bit
// row dwords resolved to the x64 record fields. Handlers with client refresh calls or DLL side effects follow.
#include "CoA/Source/Modules/ClientDbcPatch.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/PatchData.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/Magic/MagicClient/SpellTable.h"
#include "WoW/Source/Magic/MagicClient/SpellVisuals.h"
#include "WoW/Source/Object/Container.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Container_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/SimpleScript.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/SoundInterface/SoundInterface2.h"
#include "WoW/Source/Ui/AchievementInfo.h"
#include "WoW/Source/Ui/ContainerFrame.h"
#include "WoW/Source/Ui/SpellBookFrame.h"
#include <Windows.h>
#include <cstring>
#include <functional>
#include <map>
#include <type_traits>
#include <vector>

#define COA_CLIENT_DBC_PATCH_INCLUDES
#include "CoA/Source/Modules/ClientDbcPatch.generated.inc"
#undef COA_CLIENT_DBC_PATCH_INCLUDES

namespace {
    // A wire dword into a non-pointer field (byte offset `delta` into it, for arrays and multi-dword fields).
    template <class F>
    void Put(F& field, uint32_t delta, uint64_t v) {
        const uint32_t x = static_cast<uint32_t>(v);
        memcpy(reinterpret_cast<uint8_t*>(&field) + delta, &x, 4);
    }

    // A string the reader allocated into a pointer field.
    template <class P>
    void PutPtr(P& slot, uint64_t v) {
        slot = reinterpret_cast<P>(static_cast<uintptr_t>(v));
    }

#define COA_CLIENT_DBC_PATCH_WRITERS
#include "CoA/Source/Modules/ClientDbcPatch.generated.inc"
#undef COA_CLIENT_DBC_PATCH_WRITERS

    // The index the original reallocates through SMemAlloc / SMemFree (0x76E540 / 0x76E5A0, "WowClientDB.h" line 0x178).
    void* IndexAlloc(size_t n) {
        return SMemAlloc(n, "WowClientDB.h", 0x178, 0);
    }

    void IndexFree(void* p) {
        SMemFree(p, nullptr, 0, 0);
    }

    // WowClientDB_RLE keeps its index inside the record block (ClientDbcPatch::Insert's ownsIndex).
    template <class DB>
    struct IsRle : std::false_type {};

    template <class T>
    struct IsRle<WowClientDB_RLE<T>> : std::true_type {};

    // The table's slot for id (WowClientDB and WowClientDB_RLE alike: the RLE slot is the stored, possibly compressed,
    // row), or nullptr.
    template <class DB>
    auto Slot(DB& db, int32_t id) -> std::remove_reference_t<decltype(db.m_recordsById[0])> {
        if (id < db.m_minID || id > db.m_maxID || !db.m_recordsById) {
            return nullptr;
        }

        return db.m_recordsById[id - db.m_minID];
    }

    // One table patch: overwrite the row with that id, or insert a new one. Returns the row; `inserted` tells which.
    struct Patch {
        const char* program;
        std::function<void*(const std::vector<uint64_t>& v, bool& inserted)> apply;
    };

    std::map<uint32_t, Patch>& Patches() {
        static std::map<uint32_t, Patch> patches;
        return patches;
    }

    template <class DB, class Rec>
    void Add(uint32_t opcode, DB& db, const char* program, void (*write)(Rec*, const uint64_t*)) {
        Patches()[opcode] = { program, [&db, write](const std::vector<uint64_t>& v, bool& inserted) -> void* {
            // The id is whatever lands in the row's first dword.
            alignas(Rec) uint8_t probe[sizeof(Rec)] = {};
            write(reinterpret_cast<Rec*>(probe), v.data());
            int32_t id;
            memcpy(&id, probe, 4);

            if (Rec* row = Slot(db, id)) {
                write(row, v.data());
                inserted = false;
                return row;
            }

            Rec* row = static_cast<Rec*>(operator new(sizeof(Rec)));
            memset(static_cast<void*>(row), 0, sizeof(Rec));
            write(row, v.data());
            CoA::ClientDbcPatch::Insert(db, id, row, IndexAlloc, IndexFree, !IsRle<DB>::value);
            inserted = true;
            return row;
        } };
    }

    // Follow-ups the original runs after some patches.
    using After = void (*)(void* row, bool inserted);

    std::map<uint32_t, After>& Afters() {
        static std::map<uint32_t, After> afters;
        return afters;
    }

    void* Apply(uint32_t opcode, CDataStore* msg, bool& inserted) {
        const Patch& patch = Patches()[opcode];
        return patch.apply(CoA::ClientDbcPatch::ReadValues(patch.program, msg), inserted);
    }

    int32_t OnPatch(void*, uint32_t opcode, uint32_t, CDataStore* msg) {
        if (!Patches().count(opcode)) {
            return 1;
        }

        bool inserted = false;
        void* row = Apply(opcode, msg, inserted);
        auto after = Afters().find(opcode);

        if (after != Afters().end()) {
            after->second(row, inserted);
        }

        return 1;
    }

    // The record's first dword (its id), left unread.
    uint32_t PeekU32(CDataStore* msg) {
        const uint32_t v = CoA::Read::U32(msg);
        msg->m_read -= 4;
        return v;
    }

    // FUN_1008e3d0: for each equipped bag that exists, its container re-resolves every slot's item pointer. (The
    // original first looks up the 39 inventory / pack slot items and discards the results.)
    void RefreshInventory(CGPlayer_C*) {
        for (uint32_t bag = 0; bag < 4; ++bag) {
            CGContainer_C* container = static_cast<CGContainer_C*>(ClntObjMgrObjectPtr(CGContainerInfo::m_containers[bag], TYPE_CONTAINER, __FILE__, __LINE__));

            if (!container) {
                continue;
            }

            CGBag_C* items = container->GetBag();

            if (!items) {
                continue;
            }

            for (uint32_t i = 0; i < container->m_container->m_containerFieldNumSlots; ++i) {
                items->GetItemPointer(static_cast<int32_t>(i));
            }
        }
    }

    // 0x932 ITEM (FUN_1022d960): id 0 is refused (returns 0, nothing stored); an OVERWRITTEN row is followed by
    // RefreshInventory(player).
    int32_t On932(void*, uint32_t opcode, uint32_t, CDataStore* msg) {
        const uint32_t id = PeekU32(msg);

        if (id == 0) {
            msg->m_read += 0x20;
            return 0;
        }

        bool inserted = false;
        Apply(opcode, msg, inserted);

        if (!inserted) {
            if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
                RefreshInventory(player);
            }
        }

        return 1;
    }

    uint8_t s_spellbookDirty = 0;   // DAT_10BE24E4: only ever written (0x934 / 0x92A)
    uint8_t s_spellDirty2 = 0;      // DAT_10BE24E5: only ever written (0x92A)

    // 0x934 SKILL_LINE_ABILITY (FUN_10230100). Overwrite: the OLD spell leaves the spell -> ability map before the row
    // is overwritten. Both paths then map the new spell, set the spell-book flag while in world and the client's
    // skill-line link info dirty; an overwrite also re-learns the spell (DelKnownSpell + AddKnownSpell) when the spell
    // book has it.
    int32_t On934(void*, uint32_t opcode, uint32_t, CDataStore* msg) {
        const int32_t id = static_cast<int32_t>(PeekU32(msg));
        const CoA::ClientDbcPatch::Deps& deps = CoA::ClientDbcPatch::Dependencies();

        if (const SkillLineAbilityRec* old = Slot(g_skillLineAbilityDB, id)) {
            if (deps.skillLineAbilityErase) {
                deps.skillLineAbilityErase(static_cast<uint32_t>(old->m_spell));
            }
        }

        bool inserted = false;
        const SkillLineAbilityRec* row = static_cast<const SkillLineAbilityRec*>(Apply(opcode, msg, inserted));
        const int32_t spell = row->m_spell;

        if (deps.skillLineAbilityUpdate) {
            deps.skillLineAbilityUpdate(static_cast<uint32_t>(spell));
        }

        if (CoA::ClientState::InWorld()) {
            s_spellbookDirty = 1;
        }

        SpellTableMarkSkillLineLinkInfoDirty();

        if (inserted) {
            return 1;
        }

        if (GetSpellSlot(spell, 0) != -1) {
            CGSpellBook::DelKnownSpell(spell, 0);
            CGSpellBook::AddKnownSpell(spell, 1, 0);
        }

        return 1;
    }

    // 0x92A SPELL (FUN_10231de0): the 0x2A8-byte Spell.dbc image, then four sized strings stored at +0x220, +0x228,
    // +0x224, +0x22C. Spell modifiers the client cannot hold -- an effect applying aura 0x6B / 0x6C (add flat / pct
    // modifier) with misc value > 30 -- become aura 4 (dummy). The row is written compressed (CompressRow) while the
    // client compresses its tables, else copied. An overwrite in world sets the two DLL flags; a new row is inserted.
    // (Compressed, an overwrite writes the new row over the old one's bytes, whatever their length -- as the original.)
    int32_t On92A(void*, uint32_t, uint32_t, CDataStore* msg) {
        uint32_t image[0x2A8 / 4];
        CoA::Read::Data(msg, image, sizeof(image));
        const std::vector<uint64_t> strings = CoA::ClientDbcPatch::ReadValues("zzzz", msg);
        alignas(SpellRec) uint8_t buffer[sizeof(SpellRec)] = {};
        SpellRec* spell = reinterpret_cast<SpellRec*>(buffer);
        WriteSpellImage(spell, image, strings.data());

        for (int32_t i = 0; i < 3; ++i) {
            if ((spell->m_effectAura[i] == 0x6B || spell->m_effectAura[i] == 0x6C) && spell->m_effectMiscValue[i] > 0x1E) {
                spell->m_effectAura[i] = 4;
            }
        }

        const bool compressed = g_clientDbPerformCompress != 0;

        if (SpellRec* row = Slot(g_spellDB, spell->m_id)) {
            if (compressed) {
                CompressRow(buffer, sizeof(SpellRec), reinterpret_cast<uint8_t*>(row));
            } else {
                memcpy(static_cast<void*>(row), buffer, sizeof(SpellRec));
            }

            if (CoA::ClientState::InWorld()) {
                s_spellbookDirty = 1;
                s_spellDirty2 = 1;
            }

            return 1;
        }

        SpellRec* row = static_cast<SpellRec*>(operator new(sizeof(SpellRec)));
        memcpy(static_cast<void*>(row), buffer, sizeof(SpellRec));

        if (compressed) {
            CompressRow(buffer, sizeof(SpellRec), reinterpret_cast<uint8_t*>(row));
        }

        CoA::ClientDbcPatch::Insert(g_spellDB, spell->m_id, row, IndexAlloc, IndexFree, false);   // RLE: shared index block
        return 1;
    }

    // 0x95A (FUN_10233540): the record (id, 8 skipped bytes, two sized strings) is read, but its getter always misses
    // and its inserter is an empty stub -- a disabled table. Nothing is stored (the strings leak, as in the original).
    int32_t OnDisabled95A(void*, uint32_t, uint32_t, CDataStore* msg) {
        CoA::ClientDbcPatch::ReadValues("d________zz", msg);
        return 1;
    }

    void RestartSound() {   // 0x4C6230(1), Sleep(200), 0x4C82E0(1)
        SI2::ShutDown(1);
        Sleep(200);
        SI2::Init(1);
    }

    void RestartAchievements() {   // 0x5B4F00, 0x5B6DF0
        CGAchievementInfo::ShutdownGame();
        CGAchievementInfo::InitializeGame();
    }
}

void CoA::ClientDbcPatchClient::Init() {
#define COA_CLIENT_DBC_PATCH(opcode, db, Rec, program, write) Add<decltype(db), Rec>(opcode, db, program, write);
#define COA_CLIENT_DBC_PATCH_REGISTRATIONS
#include "CoA/Source/Modules/ClientDbcPatch.generated.inc"
#undef COA_CLIENT_DBC_PATCH_REGISTRATIONS
#undef COA_CLIENT_DBC_PATCH
    COA_PACKET(0x932, On932);
    COA_PACKET(0x934, On934);
    COA_PACKET(0x92A, On92A);
    COA_PACKET(0x95A, OnDisabled95A);

    // 0x966 ITEM_SET (FUN_1022e0b0): a NEW row also runs the appearance manager's FUN_100c6750.
    Afters()[0x966] = [](void*, bool inserted) {
        if (inserted && CoA::PatchData::Dependencies().rebuildOutfits) {
            CoA::PatchData::Dependencies().rebuildOutfits();
        }
    };
    // 0x930 / 0x931 SPELL_MISSILE / SPELL_MISSILE_MOTION (FUN_10232830 / FUN_10232920), either path: the simple script
    // manager is rebuilt.
    Afters()[0x930] = [](void*, bool) {
        CSimpleScriptManager::Destroy();
        CSimpleScriptManager::Create();
    };
    Afters()[0x931] = Afters()[0x930];
    // 0x92D SPELL_VISUAL_KIT (FUN_102333b0): an OVERWRITTEN row, unless flag 0x10BE3740 (nothing sets it) -> the sound
    // system restarts.
    Afters()[0x92D] = [](void*, bool inserted) {
        if (!inserted) {
            RestartSound();
        }
    };
    // 0x92E SPELL_VISUAL_KIT_MODEL_ATTACH (FUN_10233480), either path: on a development realm the model attach lookup
    // is rebuilt.
    Afters()[0x92E] = [](void*, bool) {
        if (CoA::RealmInfo::Get().Dev()) {
            SpellVisualsInitializeModelAttachLookup();
        }
    };
    // 0x936 / 0x937 / 0x938 ACHIEVEMENT / _CRITERIA / _CATEGORY: an OVERWRITTEN row -> the achievement info restarts.
    // (0x936 then rebuilds a DLL map nothing in the plain code reads.)
    Afters()[0x936] = [](void*, bool inserted) {
        if (!inserted) {
            RestartAchievements();
        }
    };
    Afters()[0x937] = Afters()[0x936];
    Afters()[0x938] = Afters()[0x936];
    // 0x98A / 0x98B SOUND_ENTRIES_ADVANCED / SOUND_ENTRIES (FUN_10230900 / FUN_10230ac0), either path: the sound system
    // restarts.
    Afters()[0x98A] = [](void*, bool) {
        RestartSound();
    };
    Afters()[0x98B] = Afters()[0x98A];
}
