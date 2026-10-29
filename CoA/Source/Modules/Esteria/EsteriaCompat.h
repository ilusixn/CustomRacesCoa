#ifndef COA_SOURCE_MODULES_ESTERIA_ESTERIACOMPAT_H
#define COA_SOURCE_MODULES_ESTERIA_ESTERIACOMPAT_H

// Glue between the 32-bit EsteriaAppearance.dll code and the decompiled x64 client: the DLL read the client's structures
// by 32-bit byte offsets and called its functions by address; here CC<T>() maps an old CCharacterComponent offset to the
// real member and N_xxxxxx() are the functions that used to sit at 0x00xxxxxx.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/ComponentCore/ComponentUtils.h"
#include "WoW/Common/TextureCache.h"
#include "WoW/Source/DB/Rec/CharSectionsRec.h"
#include "WoW/Source/Glue/CGlueLoading.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Glue/CharacterCreation.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/CharacterSelectionDisplay.h"
#include <cstring>
#include <windows.h>
#include "Engine/Source/Model2/M2Shared.h"
#include "Engine/Source/Model2/M2Data.h"

namespace CoA::Esteria::Compat {
    // The old DLL's byte offset into a CCharacterComponent (32-bit layout), as the member it named.
    inline unsigned char* CharAddr(void* object, std::size_t off) {
        auto* c = static_cast<CCharacterComponent*>(object);
        auto* data = reinterpret_cast<unsigned char*>(&c->m_data.m_preferences);
        if (off >= 0x18 && off < 0x38) return data + (off - 0x18);
        if (off == 0x38) return reinterpret_cast<unsigned char*>(&c->m_data.m_model);
        if (off == 0x3C) return reinterpret_cast<unsigned char*>(&c->m_data.m_flags);
        if (off >= 0x144 && off < 0x190) return reinterpret_cast<unsigned char*>(&c->m_data.m_geosets[(off - 0x144) / 4]);
        if (off == 0x08) return reinterpret_cast<unsigned char*>(&c->m_flags);
        if (off == 0x0C) return reinterpret_cast<unsigned char*>(&c->m_dirtySections);
        if (off == 0x10) return reinterpret_cast<unsigned char*>(&c->m_heapIndex);
        if (off >= 0x194 && off < 0x1D0) return reinterpret_cast<unsigned char*>(&c->m_baseVariation[(off - 0x194) / 12].m_texture[((off - 0x194) % 12) / 4]);
        if (off == 0x52C) return reinterpret_cast<unsigned char*>(&c->m_request);
        if (off == 0x190) return reinterpret_cast<unsigned char*>(&c->m_baseSkinTexture);
        if (off >= 0x1D0 && off < 0x1D0 + 10 * 0x3C) {
            std::size_t region = (off - 0x1D0) / 0x3C, inner = (off - 0x1D0) % 0x3C;
            if (inner < 0x1C) return reinterpret_cast<unsigned char*>(&c->m_section[region].m_layerTex[inner / 4]);
            if (inner < 0x38) return reinterpret_cast<unsigned char*>(&c->m_section[region].m_layerItemDisplayId[(inner - 0x1C) / 4]);
            return reinterpret_cast<unsigned char*>(&c->m_section[region].m_layerMask);
        }
        if (off >= 0x428 && off < 0x458) return reinterpret_cast<unsigned char*>(&c->m_itemDisplayID[(off - 0x428) / 4]);
        if (off >= 0x458 && off < 0x464) return reinterpret_cast<unsigned char*>(&c->m_handItemDisplayID[(off - 0x458) / 4]);
        std::fprintf(stderr, "Esteria: unmapped CCharacterComponent offset 0x%zX\n", off);
        std::abort();
    }

    // A texture-cache entry (the DLL's "layer", an HTEXTURECACHE) by its 32-bit offsets.
    inline unsigned char* CacheAddr(void* object, std::size_t off) {
        auto* e = static_cast<CACHEENTRY*>(object);
        if (off == 0x18) return reinterpret_cast<unsigned char*>(&e->m_asyncObject);
        if (off >= 0x1C && off < 0x24) return reinterpret_cast<unsigned char*>(&e->m_info) + (off - 0x1C);
        if (off == 0xAC) return reinterpret_cast<unsigned char*>(&e->m_data);
        if (off == 0xB0) return reinterpret_cast<unsigned char*>(&e->m_data) + sizeof(e->m_data); // the m_size / m_loadFailed word
        std::fprintf(stderr, "Esteria: unmapped CACHEENTRY offset 0x%zX\n", off);
        std::abort();
    }

    template <class T>
    T& CE(void* entry, std::size_t off) { return *reinterpret_cast<T*>(CacheAddr(entry, off)); }

    template <class T>
    T& CC(void* character, std::size_t off) { return *reinterpret_cast<T*>(CharAddr(character, off)); }


    // Glue globals the DLL read by address.
    inline void* CreationCharacter() { return CCharacterCreation::m_character; }
    inline void SetPrevSkinIndex(unsigned v) { CCharacterCreation::m_prevSkinIndex = v; }
    inline void* ChrVarTable() { return CCharacterComponent::s_chrVarArray; }
    inline unsigned* FacialHairCounts() { return CCharacterComponent::s_characterFacialHairStylesList; }
    inline unsigned SelCount() { return CCharacterSelection::s_characterList.Count(); }
    // One character-select entry as the DLL's 0x198-byte row (guid at 0, race 0x178, sex 0x17A, skin..facial hair 0x17B..0x17F,
    // the preview component at 0x188); rebuilt on each call.
    inline unsigned char* SelRow(unsigned index) {
        static thread_local unsigned char row[0x198];
        std::memset(row, 0, sizeof(row));
        if (index >= CCharacterSelection::s_characterList.Count()) return row;
        CharacterSelectionDisplay& d = CCharacterSelection::s_characterList[index];
        const CHARACTER_INFO& i = d.m_characterInfo;
        std::memcpy(row, &i.m_guid, 8);
        row[0x178] = i.m_raceID; row[0x179] = i.m_classID; row[0x17A] = i.m_sexID; row[0x17B] = i.m_skinColorID; row[0x17C] = i.m_faceID;
        row[0x17D] = i.m_hairStyleID; row[0x17E] = i.m_hairColorID; row[0x17F] = i.m_facialHairStyleID;
        void* component = d.m_component;
        std::memcpy(row + 0x188, &component, sizeof(component));
        return row;
    }

    // Objects the DLL walked by raw offsets: a CGUnit_C (type id at 0x14, descriptor fields 0xD0, component 0xB4C, player data 0x1008).
    inline unsigned UnitTypeId(void* u) { return static_cast<unsigned>(static_cast<CGUnit_C*>(u)->m_typeId); }
    inline void* UnitComponent(void* u) { return static_cast<CGUnit_C*>(u)->m_characterComponent; }
    inline unsigned char* UnitFields(void* u) { return reinterpret_cast<unsigned char*>(static_cast<CGUnit_C*>(u)->m_unit); }
    inline unsigned char* UnitObjectFields(void* u) { return reinterpret_cast<unsigned char*>(static_cast<CGUnit_C*>(u)->m_obj); }
    inline unsigned char* PlayerData(void* u) { return reinterpret_cast<unsigned char*>(static_cast<CGPlayer_C*>(u)->m_player); }

    // CM2Model / CM2Shared fields the DLL read by offset (instance: parent 0x48, flags 0x10, scene 0x28, shared 0x2C, matrix 0xB4).
    inline void* ModelParent(void* m) { return static_cast<CM2Model*>(m)->m_attachParent; }
    inline unsigned ModelFlags(void* m) { return static_cast<CM2Model*>(m)->m_fFlags; }
    inline void* ModelScene(void* m) { return static_cast<CM2Model*>(m)->m_scene; }
    inline CM2Shared* ModelShared(void* m) { return static_cast<CM2Model*>(m)->m_shared; }
    inline const float* ModelMatrix(void* m) { return reinterpret_cast<const float*>(&static_cast<CM2Model*>(m)->m_worldTransform); }

    // Memory within +-1.5 GB of `anchor`: M2 arrays are 32-bit self-relative offsets.
    inline void* AllocNear(const void* anchor, std::size_t size) {
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(anchor), step = 1u << 20;
        for (std::uintptr_t d = step; d < 0x60000000u; d += step)
            for (int sign = -1; sign <= 1; sign += 2) {
                std::uintptr_t at = sign < 0 ? (base > d ? base - d : 0) : base + d;
                if (!at) continue;
                if (void* p = VirtualAlloc(reinterpret_cast<void*>(at & ~std::uintptr_t(0xFFFF)), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) return p;
            }
        return nullptr;
    }

    // Lua (the DLL called the client's lua_* by address).
    inline int N_84DBD0(void* L) { return lua_gettop(static_cast<lua_State*>(L)); }
    inline int N_84DF20(void* L, int i) { return lua_isnumber(static_cast<lua_State*>(L), i); }
    inline double N_84E030(void* L, int i) { return lua_tonumber(static_cast<lua_State*>(L), i); }
    inline const char* N_84E0E0(void* L, int i, std::size_t* len) { return lua_tolstring(static_cast<lua_State*>(L), i, len); }
    inline void N_84E2A0(void* L, double v) { lua_pushnumber(static_cast<lua_State*>(L), v); }
    inline void N_84E350(void* L, const char* s) { lua_pushstring(static_cast<lua_State*>(L), s); }

    // CCharacterComponent members.
    inline void N_4EA2F0(void* c, unsigned v, int load, int) { static_cast<CCharacterComponent*>(c)->SetHairColor(v, load != 0, nullptr); }
    inline void N_4EA6B0(void* c, unsigned v, int load, int all, int) { static_cast<CCharacterComponent*>(c)->SetSkinColor(v, load != 0, all != 0, nullptr); }
    inline void N_4EA490(void* c, unsigned v, int load, int) { static_cast<CCharacterComponent*>(c)->SetFace(v, load != 0, nullptr); }
    inline void N_4EA3E0(void* c, unsigned v, int) { static_cast<CCharacterComponent*>(c)->SetHairStyle(v, nullptr); }
    inline void N_4EA590(void* c, unsigned v, int load, int) { static_cast<CCharacterComponent*>(c)->SetBeardStyle(v, load != 0, nullptr); }
    inline char N_4ED6D0(void* c, int wait) { return static_cast<CCharacterComponent*>(c)->ItemsLoaded(wait); }
    inline void N_4E6AE0(void* c, int keep) { CGlueLoading::StartLoad(static_cast<CCharacterComponent*>(c), keep); }

    // Texture helpers.
    inline CStatus& EsteriaStatus() { static CStatus status; return status; }
    inline void* N_4E8D30(const char* path, void*) { return CreateTexture(path, &EsteriaStatus()); }
    inline void N_47BF30(void* texture) { HandleClose(reinterpret_cast<HOBJECT>(texture)); }
    inline int N_4F2E50(void* handle, void* info, int wait) { return TextureCacheGetInfo(static_cast<HTEXTURECACHE>(handle), *static_cast<TCTEXTUREINFO*>(info), wait); }
    inline void N_4F31A0(void* handle) { TextureCacheDestroyTexture(static_cast<HTEXTURECACHE>(handle)); }
    inline void* N_4F3930(const char* path) { return TextureCacheCreateTexture(path); }

    // ComponentUtils.
    inline bool N_4F39A0(unsigned flags, unsigned selection) { return ComponentFlagsMatch(flags, static_cast<COMPONENT_SELECTION>(selection)); }
    inline unsigned N_4F3A40(unsigned context, unsigned classId) { return GetSelectionFromContext(static_cast<COMPONENT_CONTEXT>(context), classId); }
    inline unsigned N_4F3AE0(void* table, unsigned race, unsigned sex, unsigned variation) { return ComponentGetNumVariations(static_cast<st_race*>(table), race, sex, static_cast<CHARTEXTUREVARIATIONS>(variation)); }
    inline unsigned N_4F3B10(void* table, unsigned race, unsigned sex, unsigned variation, unsigned index) { return ComponentGetNumColors(static_cast<st_race*>(table), race, sex, static_cast<CHARTEXTUREVARIATIONS>(variation), index); }
    inline const CharSectionsRec* N_4F3BA0(void* table, unsigned race, unsigned sex, unsigned variation, unsigned index, unsigned color, bool* found) {
        return (ComponentGetSectionsRecord(static_cast<st_race*>(table), race, sex, static_cast<CHARTEXTUREVARIATIONS>(variation), index, color, found));
    }

    // CM2Model / CM2Scene.
    inline void N_825260(void* m, unsigned type, void* texture) { static_cast<CM2Model*>(m)->ReplaceTexture(type, static_cast<CTexture*>(texture)); }
    inline void N_82C7C0(void* m, unsigned first, unsigned last, int visible) { static_cast<CM2Model*>(m)->SetGeometryVisible(first, last, visible); }
    inline void N_8251D0(void* m, const float* position, float rotation, float scale) {
        C3Vector p{position[0], position[1], position[2]};
        static_cast<CM2Model*>(m)->SetWorldTransform(p, rotation, scale);
    }
    inline void* N_81F8F0(void* scene, const char* file, unsigned flags) { return static_cast<CM2Scene*>(scene)->CreateModel(file, flags); }
    inline void N_824ED0(void* m) { static_cast<CM2Model*>(m)->Release(); }
    inline int N_8273D0(void* m, unsigned id) { return static_cast<CM2Model*>(m)->HasAttachment(id); }
    inline void N_8274F0(void* m) { static_cast<CM2Model*>(m)->DetachFromParent(); }
    inline void N_831630(void* m, void* parent, unsigned id, void* position, int toRoot) {
        static_cast<CM2Model*>(m)->AttachToParent(static_cast<CM2Model*>(parent), id, static_cast<const C3Vector*>(position), toRoot);
    }
    inline void N_832AB0(void* m, int bone, int sequence, int variation, int time, float speed, int blend, int primary) {
        static_cast<CM2Model*>(m)->SetBoneSequence(bone, sequence, variation, time, speed, blend, primary);
    }

    // Storm.
    inline void* N_76E540(unsigned bytes, const char* file, unsigned line, unsigned flags) { return SMemAlloc(bytes, file, line, flags); }
    inline void N_47AFE0(void* store, unsigned byte) { static_cast<CDataStore*>(store)->Put(static_cast<uint8_t>(byte)); }
}

#endif
