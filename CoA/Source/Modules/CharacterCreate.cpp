// AscCharacterCreate.cpp, data half: the realm creation gates and the archetype / class-guide lookups over the
// CharacterCreation*.dbc tables. The creation manager (static 0x10BDB700, accessor FUN_100d5070) keeps sorted
// indexes built on first use -- FUN_1018bd40 for archetypes (+0x2CC dirty), FUN_1018c240 for the class guide (+0x31C
// dirty) -- and the preview camera at +0x398..+0x3A0.
//
//   ArchetypeRoles        +0x48 str, +0x4C/+0x50/+0x54 loc, +0x58 order
//   ArchetypeCategories   +4 role, +0x4C/+0x50 str, +0x54/+0x58/+0x5C loc, +0x60 order
//   Archetypes            +4 category, +8 str, +0xC..+0x1C five spells, +0x20 str, +0x24/+0x30 three strings each,
//                         +0x3C..+0x48 str, +0x4C/+0x50/+0x54 loc, +0x58..+0x68 the five spells' descriptions (loc),
//                         +0x6C str, +0x70 order
//   ClassGuideRoles       +4/+8 str, +0xC/+0x10 loc, +0x14 order
//   ClassGuideSubroles    +4 role, +8/+0xC/+0x10 str, +0x14/+0x18 loc, +0x1C order
//   ClassGuideSubroleClasses +4 subrole, +8 class, +0xC order
//
// The preview is three override layers of 0xB0 bytes at +0x14:
//   layer 0  rebuilt by FUN_1018c8e0 from the selected archetype (FUN_1018a330) or the previewed class outfit
//            (FUN_1018aa90) -- CharacterCreation{ArchetypeRoles,ArchetypeCategories,ArchetypeDetails,ClassDetails,
//            ShapeshiftDetails}.dbc
//   layer 1  what the bindings set (read FUN_1018b000, written FUN_1018f850)
//   layer 2  nothing in the plain code writes it; merged all the same
// FUN_1018d080 merges them in order into +0x224; FUN_1018c9d0 stores the merge, applies it to the model and fires
// CHARACTER_CREATE_PREVIEW_CHANGED when it differs from the last one.
#include "CoA/Source/Modules/CharacterCreate.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

const CoA::CharacterCreate::Transform CoA::CharacterCreate::kIdentity = { { 0, 0, 0 }, { 0, 0, 0 }, 1.0f };

namespace {
    using CoA::CharacterCreate::Merged;
    using CoA::CharacterCreate::Transform;
    using CoA::CharacterCreate::kIdentity;
    using CoA::Dbc::Table;
    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushBool;
    using CoA::Script::ReadNumber;
    using CoA::Script::ToInt;

    Table& Roles() { return CoA::Dbc::Get("DBFilesClient\\CharacterCreationArchetypeRoles.dbc"); }
    Table& Categories() { return CoA::Dbc::Get("DBFilesClient\\CharacterCreationArchetypeCategories.dbc"); }
    Table& Archetypes() { return CoA::Dbc::Get("DBFilesClient\\CharacterCreationArchetypes.dbc"); }
    Table& GuideRoles() { return CoA::Dbc::Get("DBFilesClient\\CharacterCreationClassGuideRoles.dbc"); }
    Table& GuideSubroles() { return CoA::Dbc::Get("DBFilesClient\\CharacterCreationClassGuideSubroles.dbc"); }
    Table& GuideSubroleClasses() { return CoA::Dbc::Get("DBFilesClient\\CharacterCreationClassGuideSubroleClasses.dbc"); }

    const char* const kArchetypeDetails = "DBFilesClient\\CharacterCreationArchetypeDetails.dbc";
    const char* const kClassDetails = "DBFilesClient\\CharacterCreationClassDetails.dbc";
    const char* const kShapeshiftDetails = "DBFilesClient\\CharacterCreationShapeshiftDetails.dbc";

    uint32_t U32(const uint8_t* r, uint32_t off) {
        return Table::U32(r, off);
    }

    const float kDegToRad = 0.017453292f;         // 0x10B2CBF8
    const float kRadToDeg = 57.2957763671875f;    // 0x10B38974

    template <class T> struct Opt {
        T v{};
        bool set = false;
    };

    struct Vec3 {
        float v[3];
    };

    // One override layer (0xB0).
    struct Layer {
        Opt<uint32_t> spell;
        Opt<uint32_t> anim;
        Opt<uint32_t> petCreature;
        Opt<uint32_t> shapeshift;
        Opt<uint32_t> petDisplay;
        Opt<bool> weaponsDrawn;
        Opt<std::vector<uint32_t>> items;
        Opt<uint32_t> unk248;
        Opt<Transform> character;
        Opt<Transform> shape;
        Opt<Vec3> camera;
        Opt<float> facing;
        Opt<bool> undress;
        Opt<uint32_t> cast;
        Opt<uint32_t> petCast;
    };

    struct Manager {
        uint8_t mode = 0;                         // +0x05: 0 none, 1 archetype, 2 class outfit
        uint32_t selection[3] = { 0, 0, 0 };      // +0x08 role, +0x0C category, +0x10 archetype
        Layer layers[3];                          // +0x14
        Merged merged;                            // +0x224
        uint32_t classPreview = 0;                // +0x2B0
        std::string classPreviewName;             // +0x2B4
        bool archetypesDirty = true;              // +0x2CC
        std::vector<uint32_t> roles;
        std::unordered_map<uint32_t, std::vector<uint32_t>> categoriesByRole;
        std::unordered_map<uint32_t, std::vector<uint32_t>> archetypesByCategory;
        bool guideDirty = true;                   // +0x31C
        std::vector<uint32_t> guideRoles;
        std::unordered_map<uint32_t, std::vector<uint32_t>> subrolesByRole;
        std::unordered_map<uint32_t, std::vector<uint32_t>> classesBySubrole;
        bool classNamesDirty = true;              // +0x36C
        std::unordered_map<std::string, uint32_t> classByName;   // upper-case ChrClasses filename -> id
        float camera[3] = { 0, 0, 0 };            // +0x398: the camera the glue last asked for
        float zoomOrigin[3] = { 0, 0, 0 };        // +0x3A4
        Opt<uint32_t> race;                       // +0x3B0
        Opt<uint32_t> sex;                        // +0x3B8
        bool hadComponent = false;                // +0x3C0
    };

    Manager s_mgr;

    CoA::CharacterCreate::Preview& Backend() {
        return CoA::CharacterCreate::Backend();
    }

    // Every row id of a table in id order.
    template <class Fn> void ForEachRow(Table& t, Fn fn) {
        if (!t.Loaded()) {
            return;
        }

        for (uint32_t id = t.MinId(); id <= t.MaxId(); ++id) {
            if (const uint8_t* r = t.Row(id)) {
                fn(id, r);
            }

            if (id == 0xFFFFFFFF) {
                break;
            }
        }
    }

    void SortBy(Table& t, std::vector<uint32_t>& ids, uint32_t orderOff, bool tieById) {
        std::sort(ids.begin(), ids.end(), [&](uint32_t a, uint32_t b) {
            uint32_t oa = U32(t.Row(a), orderOff);
            uint32_t ob = U32(t.Row(b), orderOff);

            if (tieById && oa == ob) {
                return a < b;
            }

            return oa < ob;
        });
    }

    // FUN_1018bd40
    void BuildArchetypes() {
        s_mgr.roles.clear();
        s_mgr.categoriesByRole.clear();
        s_mgr.archetypesByCategory.clear();
        ForEachRow(Roles(), [](uint32_t id, const uint8_t*) { s_mgr.roles.push_back(id); });
        SortBy(Roles(), s_mgr.roles, 0x58, false);   // FUN_10187c20
        ForEachRow(Categories(), [](uint32_t id, const uint8_t* r) { s_mgr.categoriesByRole[U32(r, 4)].push_back(id); });

        for (auto& kv : s_mgr.categoriesByRole) {
            SortBy(Categories(), kv.second, 0x60, false);   // FUN_10188110
        }

        ForEachRow(Archetypes(), [](uint32_t id, const uint8_t* r) { s_mgr.archetypesByCategory[U32(r, 4)].push_back(id); });

        for (auto& kv : s_mgr.archetypesByCategory) {
            SortBy(Archetypes(), kv.second, 0x70, false);   // FUN_10188600
        }

        s_mgr.archetypesDirty = false;
    }

    // FUN_1018c240
    void BuildClassGuide() {
        s_mgr.guideRoles.clear();
        s_mgr.subrolesByRole.clear();
        s_mgr.classesBySubrole.clear();
        ForEachRow(GuideRoles(), [](uint32_t id, const uint8_t*) { s_mgr.guideRoles.push_back(id); });
        SortBy(GuideRoles(), s_mgr.guideRoles, 0x14, true);   // FUN_10187e90
        ForEachRow(GuideSubroles(), [](uint32_t id, const uint8_t* r) { s_mgr.subrolesByRole[U32(r, 4)].push_back(id); });

        for (auto& kv : s_mgr.subrolesByRole) {
            SortBy(GuideSubroles(), kv.second, 0x1C, true);   // FUN_10188380
        }

        std::unordered_map<uint32_t, std::vector<const uint8_t*>> rows;   // subrole -> rows
        ForEachRow(GuideSubroleClasses(), [&](uint32_t, const uint8_t* r) { rows[U32(r, 4)].push_back(r); });

        for (auto& kv : rows) {
            std::sort(kv.second.begin(), kv.second.end(), [](const uint8_t* a, const uint8_t* b) {   // FUN_10188870
                if (U32(a, 0xC) == U32(b, 0xC)) {
                    return U32(a, 0) < U32(b, 0);
                }

                return U32(a, 0xC) < U32(b, 0xC);
            });
            std::vector<uint32_t>& classes = s_mgr.classesBySubrole[kv.first];

            for (const uint8_t* r : kv.second) {
                classes.push_back(U32(r, 8));
            }
        }

        s_mgr.guideDirty = false;
    }

    // FUN_1009a460
    void PushIds(lua_State* L, const std::vector<uint32_t>& v) {
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(v[i]));
            lua_settable(L, -3);
        }
    }

    int PushList(lua_State* L, std::unordered_map<uint32_t, std::vector<uint32_t>>& map) {
        uint32_t key;

        if (!ReadNumber(L, key)) {
            return 0;
        }

        auto it = map.find(key);
        PushIds(L, it == map.end() ? std::vector<uint32_t>() : it->second);
        return 1;
    }

    void PushStrs(lua_State* L, Table& t, const uint8_t* r, std::initializer_list<uint32_t> offs) {
        for (uint32_t off : offs) {
            lua_pushstring(L, t.Str(r, off));
        }
    }

    // ---- gates (RealmInfo +0x44 dev, +0x46 CoA, +0x47 wildcard) ------------------------------------------------
    bool Dev() { return CoA::RealmInfo::Get().gates[4] != 0; }       // FUN_102fc640
    bool CoAGate() { return CoA::RealmInfo::Get().gates[6] != 0; }   // FUN_102fc630
    bool WCRGate() { return CoA::RealmInfo::Get().gates[7] != 0; }   // FUN_102fc6a0
    bool CoAAllowed() { return CoAGate() || Dev(); }                 // FUN_1018ae90
    bool WCRAllowed() { return WCRGate() || Dev(); }                 // FUN_1018aec0

    int CanCreateCoA(lua_State* L) {
        PushBool(L, CoAAllowed());
        return 1;
    }

    int CanCreateHero(lua_State* L) {
        PushBool(L, !CoAGate() && !WCRGate());
        return 1;
    }

    int CanCreateWCR(lua_State* L) {
        PushBool(L, WCRAllowed());
        return 1;
    }

    // FUN_1018d550: Hero (10) where neither CoA nor wildcard is on; the 21 CoA classes where CoA is allowed; the
    // stock classes except Death Knight where wildcard is allowed.
    int CanCreateClass(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        uint32_t cls = static_cast<uint32_t>(ToInt(CheckNumber(L, 1))) & 0xFF;
        bool ok = false;

        if (cls <= 0x20) {
            if (cls == 10 && !CoAGate() && !WCRGate()) {
                ok = true;
            } else if (cls >= 12 && cls <= 32) {
                ok = CoAAllowed();
            } else if (cls >= 1 && cls <= 11 && cls != 6 && cls != 10) {
                ok = WCRAllowed();
            }
        }

        PushBool(L, ok);
        return 1;
    }

    // ---- archetypes --------------------------------------------------------------------------------------------
    int GetArchetypeRoles(lua_State* L) {   // FUN_1018deb0
        if (s_mgr.archetypesDirty) {
            BuildArchetypes();
        }

        PushIds(L, s_mgr.roles);
        return 1;
    }

    int GetArchetypeCategories(lua_State* L) {   // FUN_1018d980 (roleId)
        if (s_mgr.archetypesDirty) {
            BuildArchetypes();
        }

        return PushList(L, s_mgr.categoriesByRole);
    }

    int GetArchetypes(lua_State* L) {   // FUN_1018e080 (categoryId)
        if (s_mgr.archetypesDirty) {
            BuildArchetypes();
        }

        return PushList(L, s_mgr.archetypesByCategory);
    }

    int GetArchetypeRoleInfo(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* r = Roles().Row(id);

        if (!r) {
            return CoA::Script::PushNils(L, 4);
        }

        PushStrs(L, Roles(), r, { 0x48, 0x4C, 0x50, 0x54 });
        return 4;
    }

    int GetArchetypeCategoryInfo(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* r = Categories().Row(id);

        if (!r) {
            return CoA::Script::PushNils(L, 5);
        }

        PushStrs(L, Categories(), r, { 0x4C, 0x50, 0x54, 0x58, 0x5C });
        return 5;
    }

    // FUN_1018dbb0: name, {spells}, primary stat, {weapon types}, {armor types}, icon, three strings, the three
    // localized texts, cinematic.
    int GetArchetypeInfo(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        Table& t = Archetypes();
        const uint8_t* r = t.Row(id);

        if (!r) {
            return CoA::Script::PushNils(L, 13);
        }

        lua_pushstring(L, t.Str(r, 8));
        lua_createtable(L, 0, 5);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < 5; ++i) {
            if (uint32_t spell = U32(r, 0xC + i * 4)) {
                lua_pushnumber(L, static_cast<double>(i + 1));
                lua_pushinteger(L, static_cast<int32_t>(spell));
                lua_settable(L, -3);
            }
        }

        lua_pushstring(L, t.Str(r, 0x20));

        for (uint32_t base : { 0x24u, 0x30u }) {   // FUN_100d1de0
            lua_createtable(L, 0, 3);
            lua_checkstack(L, 2);

            for (uint32_t i = 0; i < 3; ++i) {
                lua_pushnumber(L, static_cast<double>(i + 1));
                lua_pushstring(L, t.Str(r, base + i * 4));
                lua_settable(L, -3);
            }
        }

        PushStrs(L, t, r, { 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x6C });
        return 13;
    }

    // FUN_1018df30: (archetypeId, spellId) -> that spell's description, nil when empty or absent.
    int GetArchetypeSpellDescription(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        uint32_t id = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        uint32_t spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        Table& t = Archetypes();

        if (const uint8_t* r = t.Row(id)) {
            for (uint32_t i = 0; i < 5; ++i) {
                if (U32(r, 0xC + i * 4) == spell) {
                    const char* s = t.Str(r, 0x58 + i * 4);

                    if (*s) {
                        lua_pushstring(L, s);
                        return 1;
                    }

                    break;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // ---- class guide -------------------------------------------------------------------------------------------
    int GetClassGuideRoles(lua_State* L) {   // FUN_1018e390
        if (s_mgr.guideDirty) {
            BuildClassGuide();
        }

        PushIds(L, s_mgr.guideRoles);
        return 1;
    }

    int GetClassGuideSubroles(lua_State* L) {   // FUN_1018e650 (roleId)
        if (s_mgr.guideDirty) {
            BuildClassGuide();
        }

        return PushList(L, s_mgr.subrolesByRole);
    }

    int GetClassGuideSubroleClasses(lua_State* L) {   // FUN_1018e410 (subroleId)
        if (s_mgr.guideDirty) {
            BuildClassGuide();
        }

        return PushList(L, s_mgr.classesBySubrole);
    }

    int GetClassGuideRoleInfo(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* r = GuideRoles().Row(id);

        if (!r) {
            return CoA::Script::PushNils(L, 4);
        }

        PushStrs(L, GuideRoles(), r, { 4, 8, 0xC, 0x10 });
        return 4;
    }

    int GetClassGuideSubroleInfo(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* r = GuideSubroles().Row(id);

        if (!r) {
            return CoA::Script::PushNils(L, 6);
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(U32(r, 4)));
        PushStrs(L, GuideSubroles(), r, { 8, 0xC, 0x10, 0x14, 0x18 });
        return 6;
    }

    int GetCameraPosition(lua_State* L) {
        for (float v : s_mgr.camera) {
            lua_pushnumber(L, static_cast<double>(v));
        }

        return 3;
    }

    // ==== preview ===============================================================================================
    template <class T> void Set(Opt<T>& o, const T& v) {
        o.v = v;
        o.set = true;
    }

    // The component's race and sex; race 0 / sex 2 without a component.
    bool RaceSex(uint32_t& race, uint32_t& sex) {
        race = 0;
        sex = 2;
        return Backend().raceSex && Backend().raceSex(race, sex);
    }

    // The five preview columns every source row carries at `base`: spell, animation, pet creature, shapeshift
    // creature, weapons-drawn flag, then twelve item ids.
    void FillCommon(Layer& l, const uint8_t* r, uint32_t base) {
        Set(l.spell, U32(r, base));
        Set(l.anim, U32(r, base + 4));
        Set(l.petCreature, U32(r, base + 8));
        Set(l.shapeshift, U32(r, base + 0xC));
        Set(l.weaponsDrawn, U32(r, base + 0x10) != 0);
        std::vector<uint32_t> items(12);
        memcpy(items.data(), r + base + 0x14, 12 * 4);
        Set(l.items, items);
    }

    // ShapeshiftDetails (creature, race): +0xC the shapeshift model's transform, +0x28 its spell, +0x2C its
    // animation; +0x30 the character's transform, +0x4C / +0x50 the character's spell / animation.
    bool ApplyShapeshift(Layer& l) {
        if (!l.shapeshift.set || l.shapeshift.v == 0) {
            return false;
        }

        uint32_t race;
        uint32_t sex;
        RaceSex(race, sex);
        const uint8_t* s = CoA::CharacterCreate::FindDetailsRow(kShapeshiftDetails, { l.shapeshift.v, race });

        if (!s) {
            return false;
        }

        Set(l.spell, U32(s, 0x4C));
        Set(l.anim, U32(s, 0x50));
        Set(l.character, CoA::CharacterCreate::TransformAt(s, 0x30));
        Set(l.shape, CoA::CharacterCreate::TransformAt(s, 0xC));
        return true;
    }

    // FUN_1018a330: archetype (details per race / sex), else category, else role.
    Layer ArchetypeLayer() {
        Layer l;
        const uint8_t* details = nullptr;

        if (uint32_t archetype = s_mgr.selection[2]) {
            if (Archetypes().Row(archetype)) {
                uint32_t race;
                uint32_t sex;
                RaceSex(race, sex);
                details = CoA::CharacterCreate::FindDetailsRow(kArchetypeDetails, { archetype, race, sex });

                if (details) {
                    FillCommon(l, details, 0x2C);
                }
            }
        } else if (uint32_t category = s_mgr.selection[1]) {
            const uint8_t* r = Categories().Row(category);

            if (!r) {
                return l;
            }

            FillCommon(l, r, 8);
            Set(l.character, kIdentity);
        } else if (uint32_t role = s_mgr.selection[0]) {
            const uint8_t* r = Roles().Row(role);

            if (!r) {
                return l;
            }

            FillCommon(l, r, 4);
            Set(l.character, kIdentity);
        }

        if (ApplyShapeshift(l)) {
            return l;
        }

        if (details) {
            Set(l.character, CoA::CharacterCreate::TransformAt(details, 0x10));
        }

        return l;
    }

    // FUN_1018aa90: the previewed class's ClassDetails row for this race / sex.
    Layer ClassLayer() {
        Layer l;

        if (s_mgr.classPreview == 0) {
            return l;
        }

        uint32_t race;
        uint32_t sex;
        RaceSex(race, sex);
        const uint8_t* d = CoA::CharacterCreate::FindDetailsRow(kClassDetails, { s_mgr.classPreview, race, sex });

        if (!d) {
            return l;
        }

        FillCommon(l, d, 0x2C);

        if (ApplyShapeshift(l)) {
            return l;
        }

        Set(l.character, CoA::CharacterCreate::TransformAt(d, 0x10));
        return l;
    }

    // FUN_1018c8e0
    void RebuildLayer0() {
        if (s_mgr.mode == 1) {
            s_mgr.layers[0] = ArchetypeLayer();
        } else if (s_mgr.mode == 2) {
            s_mgr.layers[0] = ClassLayer();
        } else {
            s_mgr.layers[0] = Layer();
        }
    }

    // FUN_1018d080
    Merged Merge() {
        Merged m;

        for (const Layer& l : s_mgr.layers) {
            if (l.spell.set) m.spell = l.spell.v;
            if (l.anim.set) m.anim = l.anim.v;
            if (l.petCreature.set) m.petCreature = l.petCreature.v;
            if (l.shapeshift.set) m.shapeshift = l.shapeshift.v;
            if (l.petDisplay.set) m.petDisplay = l.petDisplay.v;
            if (l.weaponsDrawn.set) m.weaponsDrawn = l.weaponsDrawn.v;
            if (l.items.set) m.items = l.items.v;
            if (l.unk248.set) m.unk248 = l.unk248.v;

            if (l.character.set) {
                m.hasCharacter = true;
                m.character = l.character.v;
            }

            if (l.shape.set) {
                m.hasShape = true;
                m.shape = l.shape.v;
            }

            if (l.camera.set) {
                // An all-zero camera clears the override and leaves the last coordinates in place.
                const float* v = l.camera.v.v;

                if (v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f) {
                    m.hasCamera = false;
                } else {
                    m.hasCamera = true;
                    std::copy(v, v + 3, m.camera);
                }
            }

            if (l.facing.set) {
                m.hasFacing = true;
                m.facing = l.facing.v;
            }

            if (l.undress.set) {
                m.hasUndress = true;
                m.undress = l.undress.v;
            }

            if (l.cast.set) m.cast = l.cast.v;
            if (l.petCast.set) m.petCast = l.petCast.v;
        }

        return m;
    }

    // FUN_1018a2a0
    bool SameTransform(const Transform& a, const Transform& b) {
        for (int i = 0; i < 3; ++i) {
            if (a.pos[i] != b.pos[i] || a.rot[i] != b.rot[i]) {
                return false;
            }
        }

        return a.scale == b.scale;
    }

    // FUN_1018a0f0
    bool Same(const Merged& a, const Merged& b) {
        if (a.spell != b.spell || a.anim != b.anim || a.petCreature != b.petCreature || a.shapeshift != b.shapeshift || a.petDisplay != b.petDisplay ||
            a.weaponsDrawn != b.weaponsDrawn || a.items != b.items || a.unk248 != b.unk248) {
            return false;
        }

        if (a.hasCharacter != b.hasCharacter || (a.hasCharacter && !SameTransform(a.character, b.character))) {
            return false;
        }

        if (a.hasShape != b.hasShape || (a.hasShape && !SameTransform(a.shape, b.shape))) {
            return false;
        }

        if (a.hasCamera != b.hasCamera || (a.hasCamera && (a.camera[0] != b.camera[0] || a.camera[1] != b.camera[1] || a.camera[2] != b.camera[2]))) {
            return false;
        }

        if (a.hasFacing != b.hasFacing || (a.hasFacing && a.facing != b.facing)) {
            return false;
        }

        if (a.hasUndress != b.hasUndress || (a.hasUndress && a.undress != b.undress)) {
            return false;
        }

        return a.cast == b.cast && a.petCast == b.petCast;
    }

    void ApplyToModel(const Merged& m) {
        if (Backend().apply) {
            Backend().apply(m);
        }
    }

    // FUN_1018fa50
    void PreviewChanged() {
        CoA::Events::Signal("CHARACTER_CREATE_PREVIEW_CHANGED", nullptr);
    }

    // FUN_1018fb50
    void SelectionChanged() {
        CoA::Events::Signal("CHARACTER_CREATE_SELECTION_CHANGED", "%u%u%u", s_mgr.selection[0], s_mgr.selection[1], s_mgr.selection[2]);
    }

    // FUN_1018c9d0: store the merge and apply it; announce when it changed.
    void Update() {
        Merged m = Merge();
        bool same = Same(s_mgr.merged, m);
        s_mgr.merged = std::move(m);
        ApplyToModel(s_mgr.merged);

        if (!same) {
            PreviewChanged();
        }
    }

    // The tail of FUN_1018b7c0: apply only when the merge changed.
    void UpdateIfChanged() {
        Merged m = Merge();
        bool same = Same(s_mgr.merged, m);
        s_mgr.merged = std::move(m);

        if (!same) {
            ApplyToModel(s_mgr.merged);
            PreviewChanged();
        }
    }

    // Head of FUN_1018b7c0 / FUN_1018b8f0.
    void SnapshotComponent() {
        uint32_t race;
        uint32_t sex;
        s_mgr.hadComponent = RaceSex(race, sex);
        Set(s_mgr.race, race);
        Set(s_mgr.sex, sex);
        RebuildLayer0();
    }

    // FUN_1018f850(1, ...)
    void WriteLayer1(const Layer& l) {
        s_mgr.layers[1] = l;
    }

    // ---- customization cycling ---------------------------------------------------------------------------------
    bool Cycle(uint32_t type, int32_t dir) {
        return dir != 0 && Backend().cycle && Backend().cycle(type, dir);
    }

    // FUN_1032dbf0 (the vanity manager's customization-lock query): in this build it answers "not a lockable
    // option" for everything, so the skip below never steps.
    bool CustomizationLock(uint32_t type, bool& lockable, bool& unlocked, uint32_t& requirement) {
        lockable = false;
        unlocked = true;
        requirement = 0;
        return true;
    }

    // FUN_1018aef0: step past locked options, at most 200.
    bool SkipLocked(uint32_t type, int32_t dir) {
        bool stepped = false;

        for (uint32_t n = 0;;) {
            bool lockable = false;
            bool unlocked = true;
            uint32_t requirement = 0;

            if (!CustomizationLock(type, lockable, unlocked, requirement) || !lockable || unlocked) {
                return stepped;
            }

            if (!Cycle(type, dir)) {
                return stepped;
            }

            stepped = true;

            if (++n > 199) {
                return true;
            }
        }
    }

    void ReloadComponent() {
        if (Backend().reloadComponent) {
            Backend().reloadComponent();
        }
    }

    // FUN_1018d030
    void SkipAllLocked() {
        bool stepped = false;

        for (uint32_t type = 1; type <= 5; ++type) {
            if (SkipLocked(type, 1)) {
                stepped = true;
            }
        }

        if (stepped) {
            ReloadComponent();
        }
    }

    // FUN_1018b8f0
    void RefreshAfterRaceSex() {
        SnapshotComponent();
        Update();
    }

    // FUN_1018b7c0
    void RefreshIfChanged() {
        SnapshotComponent();
        UpdateIfChanged();
    }

    // FUN_1018b790, after an SMSG_PATCH_CHARACTER_CREATION_* row lands: rebuild every index and the preview.
    void OnCreationTablePatched(const uint8_t* row) {
        s_mgr.archetypesDirty = true;
        s_mgr.guideDirty = true;
        s_mgr.classNamesDirty = true;
        RebuildLayer0();
        Update();
    }

    // FUN_1018f5a0: the facing override, put straight on the models without a full update.
    void SetFacing(float radians) {
        Layer l = s_mgr.layers[1];
        Set(l.facing, radians);
        WriteLayer1(l);
        s_mgr.merged.hasFacing = true;
        s_mgr.merged.facing = radians;

        if (Backend().applyTransforms) {
            Backend().applyTransforms(s_mgr.merged);
        }
    }

    // FUN_1018f500
    void SetCamera(const float* v) {
        Layer l = s_mgr.layers[1];
        Set(l.camera, Vec3{ { v[0], v[1], v[2] } });
        WriteLayer1(l);
        Update();
    }

    // ---- bindings ----------------------------------------------------------------------------------------------
    // Layer-1 setters share one shape: copy the layer, set one override, write it back, update.
    template <class Fn> int EditLayer1(Fn fn) {
        Layer l = s_mgr.layers[1];
        fn(l);
        WriteLayer1(l);
        Update();
        return 0;
    }

    int AddPet(lua_State* L) {   // FUN_1018d490 (displayId)
        uint32_t display;

        if (ReadNumber(L, display)) {
            EditLayer1([&](Layer& l) { Set(l.petDisplay, display); });
        }

        return 0;
    }

    int PlayCastAnimation(lua_State* L) {   // FUN_1018e7a0 (spell)
        uint32_t spell;

        if (ReadNumber(L, spell)) {
            EditLayer1([&](Layer& l) { Set(l.cast, spell); });
        }

        return 0;
    }

    int PlayPetCastAnimation(lua_State* L) {   // FUN_1018e860 (spell)
        uint32_t spell;

        if (ReadNumber(L, spell)) {
            EditLayer1([&](Layer& l) { Set(l.petCast, spell); });
        }

        return 0;
    }

    int StopCastAnimation(lua_State* L) {   // FUN_1018edf0
        return EditLayer1([](Layer& l) { Set(l.cast, 0u); });
    }

    int StopPetCastAnimation(lua_State* L) {   // FUN_1018ee90
        return EditLayer1([](Layer& l) { Set(l.petCast, 0u); });
    }

    // FUN_1018d8d0: drop the weapons-drawn, item and undress overrides.
    int Dress(lua_State* L) {
        return EditLayer1([](Layer& l) {
            l.items = Opt<std::vector<uint32_t>>();
            l.weaponsDrawn.set = false;
            l.undress.set = false;
        });
    }

    // FUN_1018ef70: sheathed, no items, undressed.
    int Undress(lua_State* L) {
        return EditLayer1([](Layer& l) {
            Set(l.weaponsDrawn, false);
            Set(l.items, std::vector<uint32_t>());
            Set(l.undress, true);
        });
    }

    // Slot key of an item for TryOnItem (FUN_1018fd20): a weapon's hand slot (10001 one-hand / main, 10002 off
    // hand, 10003 two-hand, 10004 ranged, else 10000 + type); anything else its inventory type. False without a row.
    bool TryOnKey(uint32_t id, int32_t& key, bool& weapon) {
        uint32_t itemClass;
        uint32_t inv;

        if (!Backend().item || !Backend().item(id, itemClass, inv)) {
            return false;
        }

        weapon = itemClass == 2;

        if (!weapon) {
            key = static_cast<int32_t>(inv);
            return true;
        }

        switch (inv) {
        case 0x0D:
        case 0x15:
            key = 0x2711;
            break;
        case 0x16:
            key = 0x2712;
            break;
        case 0x11:
            key = 0x2713;
            break;
        case 0x0F:
        case 0x1A:
            key = 0x2714;
            break;
        default:
            key = static_cast<int32_t>(inv) + 10000;
            break;
        }

        return true;
    }

    // FUN_1018fd20: replace whatever the preview wears in the item's slot with the item; a weapon also sheathes.
    int TryOnItem(lua_State* L) {
        uint32_t id = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        int32_t key;
        bool weapon;

        if (!TryOnKey(id, key, weapon)) {
            return 0;
        }

        Layer l = s_mgr.layers[1];
        std::vector<uint32_t> items = l.items.set ? l.items.v : s_mgr.merged.items;
        items.erase(std::remove_if(items.begin(), items.end(), [&](uint32_t other) {
            int32_t otherKey;
            bool otherWeapon;
            return TryOnKey(other, otherKey, otherWeapon) && otherKey == key;
        }), items.end());
        items.push_back(id);
        Set(l.items, items);

        if (weapon) {
            Set(l.weaponsDrawn, false);
        }

        WriteLayer1(l);
        Update();
        return 0;
    }

    int ResetCameraPosition(lua_State* L) {   // FUN_1018eab0
        Layer l = s_mgr.layers[1];
        Set(l.camera, Vec3{ { 0, 0, 0 } });
        std::fill(s_mgr.zoomOrigin, s_mgr.zoomOrigin + 3, 0.0f);
        WriteLayer1(l);
        Update();
        return 0;
    }

    int SetCameraPosition(lua_State* L) {   // FUN_1018eb80 (x, y, z)
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const float v[3] = { static_cast<float>(CheckNumber(L, 1)), static_cast<float>(CheckNumber(L, 2)), static_cast<float>(CheckNumber(L, 3)) };
        SetCamera(v);
        return 0;
    }

    // The original's length: pow(d, 2) per axis rounded to float, summed y + x + z in float, then a double sqrt.
    float Length(float x, float y, float z) {
        float pz = static_cast<float>(pow(static_cast<double>(z), 2.0));
        float px = static_cast<float>(pow(static_cast<double>(x), 2.0));
        float py = static_cast<float>(pow(static_cast<double>(y), 2.0));
        float sum = py + px + pz;
        return static_cast<float>(sqrt(static_cast<double>(sum)));
    }

    // FUN_1018f070 (distance): move the camera `distance` toward the character's attachment 0x14, unless that would
    // pass it. The first zoom remembers where the camera started.
    int ZoomIn(lua_State* L) {
        float distance = static_cast<float>(CheckNumber(L, 1));
        float* o = s_mgr.zoomOrigin;

        if (o[0] == 0.0f && o[1] == 0.0f && o[2] == 0.0f) {
            std::copy(s_mgr.camera, s_mgr.camera + 3, o);
        }

        float target[3] = { 0, 0, 0 };

        if (!Backend().attachmentPosition || !Backend().attachmentPosition(0x14, target)) {
            return 0;
        }

        const float* cam = s_mgr.camera;
        float dx = cam[0] - target[0];
        float dy = cam[1] - target[1];
        float dz = cam[2] - target[2];

        if (distance <= Length(dx, dy, dz)) {
            float inv = 1.0f / static_cast<float>(sqrt(static_cast<double>(dy * dy + dx * dx + dz * dz)));
            const float v[3] = { cam[0] - dx * inv * distance, cam[1] - dy * inv * distance, cam[2] - dz * inv * distance };
            SetCamera(v);
        }

        return 0;
    }

    // FUN_1018f2e0 (distance): move the camera back toward where the zoom started, at most `distance`.
    int ZoomOut(lua_State* L) {
        float distance = static_cast<float>(CheckNumber(L, 1));
        const float* o = s_mgr.zoomOrigin;
        float unused[3];

        if ((o[0] == 0.0f && o[1] == 0.0f && o[2] == 0.0f) || !Backend().attachmentPosition || !Backend().attachmentPosition(0x14, unused)) {
            return 0;
        }

        const float* cam = s_mgr.camera;
        float dx = cam[0] - o[0];
        float dy = cam[1] - o[1];
        float dz = cam[2] - o[2];
        float len = Length(dx, dy, dz);

        if (len == 0.0f) {
            return 0;
        }

        if (distance <= len) {
            len = distance;
        }

        float inv = 1.0f / static_cast<float>(sqrt(static_cast<double>(dy * dy + dx * dx + dz * dz)));
        const float v[3] = { cam[0] - inv * dx * len, cam[1] - inv * dy * len, cam[2] - inv * dz * len };
        SetCamera(v);
        return 0;
    }

    // FUN_1018f9d0 via FUN_1018ecd0 (role, category, archetype).
    int SetSelectedArchetype(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t sel[3] = { static_cast<uint32_t>(ToInt(CheckNumber(L, 1))), static_cast<uint32_t>(ToInt(CheckNumber(L, 2))),
            static_cast<uint32_t>(ToInt(CheckNumber(L, 3))) };
        bool changed = !std::equal(sel, sel + 3, s_mgr.selection);
        std::copy(sel, sel + 3, s_mgr.selection);

        if (sel[0] == 0 && sel[1] == 0 && sel[2] == 0) {
            if (s_mgr.mode == 1) {
                s_mgr.mode = 0;
            }
        } else {
            s_mgr.mode = 1;
        }

        RebuildLayer0();
        Update();

        if (changed) {
            SelectionChanged();
        }

        return 0;
    }

    // FUN_1018b5d0
    std::string Upper(const char* s, size_t n) {
        std::string out(s, n);

        for (char& ch : out) {
            ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
        }

        return out;
    }

    // FUN_1018e920 -> FUN_1018baf0 (classFile): preview that class's outfit ("WARRIOR", case-insensitive).
    int PreviewClassOutfit(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING })) {
            return 0;
        }

        std::string name = CheckString(L, 1);

        if (name.empty()) {
            return 0;
        }

        if (s_mgr.classNamesDirty) {
            if (Backend().classFiles) {
                Backend().classFiles([](const char* file, uint32_t id) {
                    if (file && *file) {
                        s_mgr.classByName[Upper(file, strlen(file))] = id;
                    }
                });
            }

            s_mgr.classNamesDirty = false;
        }

        std::string key = Upper(name.data(), name.size());
        auto it = s_mgr.classByName.find(key);

        if (it == s_mgr.classByName.end()) {
            return 0;
        }

        s_mgr.classPreview = it->second;
        s_mgr.classPreviewName = key;
        s_mgr.mode = 2;
        RebuildLayer0();
        Update();
        return 0;
    }

    int ClearClassPreview(lua_State* L) {
        if (s_mgr.classPreview == 0 && s_mgr.classPreviewName.empty() && s_mgr.mode != 2) {
            return 0;
        }

        s_mgr.classPreview = 0;
        s_mgr.classPreviewName.clear();

        if (s_mgr.mode == 2) {
            s_mgr.mode = (s_mgr.selection[0] || s_mgr.selection[1] || s_mgr.selection[2]) ? 1 : 0;
        }

        RebuildLayer0();
        Update();
        return 0;
    }

    // FUN_1018d830 (type, direction): one step, then past locked options, then refresh.
    int CycleCustomizationSkipLocked(lua_State* L) {
        uint32_t type = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        int32_t dir = static_cast<int32_t>(CheckNumber(L, 2));

        if (dir == 0) {
            dir = 1;
        }

        if (Cycle(type, dir)) {
            SkipLocked(type, dir);
            ReloadComponent();
            RefreshIfChanged();
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CharacterCreate", "CanCreateCoA", CanCreateCoA),
        COA_NATIVE("C_CharacterCreate", "CanCreateHero", CanCreateHero),
        COA_NATIVE("C_CharacterCreate", "CanCreateWCR", CanCreateWCR),
        COA_NATIVE("C_CharacterCreate", "CanCreateClass", CanCreateClass),
        COA_NATIVE("C_CharacterCreate", "GetArchetypeRoles", GetArchetypeRoles),
        COA_NATIVE("C_CharacterCreate", "GetArchetypeCategories", GetArchetypeCategories),
        COA_NATIVE("C_CharacterCreate", "GetArchetypes", GetArchetypes),
        COA_NATIVE("C_CharacterCreate", "GetArchetypeRoleInfo", GetArchetypeRoleInfo),
        COA_NATIVE("C_CharacterCreate", "GetArchetypeCategoryInfo", GetArchetypeCategoryInfo),
        COA_NATIVE("C_CharacterCreate", "GetArchetypeInfo", GetArchetypeInfo),
        COA_NATIVE("C_CharacterCreate", "GetArchetypeSpellDescription", GetArchetypeSpellDescription),
        COA_NATIVE("C_CharacterCreate", "GetClassGuideRoles", GetClassGuideRoles),
        COA_NATIVE("C_CharacterCreate", "GetClassGuideSubroles", GetClassGuideSubroles),
        COA_NATIVE("C_CharacterCreate", "GetClassGuideSubroleClasses", GetClassGuideSubroleClasses),
        COA_NATIVE("C_CharacterCreate", "GetClassGuideRoleInfo", GetClassGuideRoleInfo),
        COA_NATIVE("C_CharacterCreate", "GetClassGuideSubroleInfo", GetClassGuideSubroleInfo),
        COA_NATIVE("C_CharacterCreate", "GetCameraPosition", GetCameraPosition),
        COA_NATIVE("C_CharacterCreate", "PlayCastAnimation", PlayCastAnimation),
        COA_NATIVE("C_CharacterCreate", "StopCastAnimation", StopCastAnimation),
        COA_NATIVE("C_CharacterCreate", "AddPet", AddPet),
        COA_NATIVE("C_CharacterCreate", "PlayPetCastAnimation", PlayPetCastAnimation),
        COA_NATIVE("C_CharacterCreate", "StopPetCastAnimation", StopPetCastAnimation),
        COA_NATIVE("C_CharacterCreate", "SetCameraPosition", SetCameraPosition),
        COA_NATIVE("C_CharacterCreate", "ResetCameraPosition", ResetCameraPosition),
        COA_NATIVE("C_CharacterCreate", "ZoomIn", ZoomIn),
        COA_NATIVE("C_CharacterCreate", "ZoomOut", ZoomOut),
        COA_NATIVE("C_CharacterCreate", "SetSelectedArchetype", SetSelectedArchetype),
        COA_NATIVE("C_CharacterCreate", "PreviewClassOutfit", PreviewClassOutfit),
        COA_NATIVE("C_CharacterCreate", "ClearClassPreview", ClearClassPreview),
        COA_NATIVE("C_CharacterCreate", "Dress", Dress),
        COA_NATIVE("C_CharacterCreate", "Undress", Undress),
        COA_NATIVE("C_CharacterCreate", "TryOnItem", TryOnItem),
        COA_NATIVE("C_CharacterCreate", "CycleCustomizationSkipLocked", CycleCustomizationSkipLocked),
    };
}

CoA::CharacterCreate::Preview& CoA::CharacterCreate::Backend() {
    static Preview preview;
    return preview;
}

const CoA::CharacterCreate::Merged& CoA::CharacterCreate::Current() {
    return s_mgr.merged;
}

// FUN_101ccf70 / FUN_101cd890 / FUN_101cdbb0 / FUN_101cdda0: the first row, in id order, whose columns 1.. equal the
// keys.
const uint8_t* CoA::CharacterCreate::FindDetailsRow(const char* table, std::initializer_list<uint32_t> keys) {
    const uint8_t* found = nullptr;
    ForEachRow(CoA::Dbc::Get(table), [&](uint32_t, const uint8_t* r) {
        if (found) {
            return;
        }

        uint32_t off = 4;

        for (uint32_t k : keys) {
            if (U32(r, off) != k) {
                return;
            }

            off += 4;
        }

        found = r;
    });
    return found;
}

// pos, rotation in degrees, scale.
CoA::CharacterCreate::Transform CoA::CharacterCreate::TransformAt(const uint8_t* row, uint32_t off) {
    Transform t;

    for (int i = 0; i < 3; ++i) {
        t.pos[i] = Table::F32(row, off + i * 4);
        t.rot[i] = Table::F32(row, off + 0xC + i * 4) * kDegToRad;
    }

    t.scale = Table::F32(row, off + 0x18);
    return t;
}

// LAB_1018e250: the facing override, else the shapeshift or character transform's yaw, in degrees.
float CoA::CharacterCreate::FacingDegrees() {
    const Merged& m = s_mgr.merged;
    float yaw = 0.0f;

    if (m.hasFacing) {
        yaw = m.facing;
    } else if (m.shapeshift != 0 && m.hasShape) {
        yaw = m.shape.rot[2];
    } else if (m.hasCharacter) {
        yaw = m.character.rot[2];
    }

    return yaw * kRadToDeg;
}

// LAB_1018ec80
void CoA::CharacterCreate::SetFacingDegrees(float degrees) {
    SetFacing(degrees * kDegToRad);
}

void CoA::CharacterCreate::AfterSetSelectedSex() {
    SkipAllLocked();
    RefreshAfterRaceSex();
}

void CoA::CharacterCreate::AfterSetSelectedRace() {
    SkipAllLocked();
    RefreshAfterRaceSex();
}

void CoA::CharacterCreate::AfterSetSelectedClass() {
    SkipAllLocked();
    RefreshIfChanged();
}

void CoA::CharacterCreate::AfterRandomize() {
    SkipAllLocked();
    RefreshIfChanged();
}

// FUN_1018af70: id 7 is the character-creation camera. Remember what the glue asked for and substitute the preview's
// camera when one is set.
void CoA::CharacterCreate::AfterCameraCoord(int32_t id, float* out) {
    if (id == 7 && out) {
        std::copy(out, out + 3, s_mgr.camera);

        if (s_mgr.merged.hasCamera) {
            std::copy(s_mgr.merged.camera, s_mgr.merged.camera + 3, out);
        }

        std::copy(out, out + 3, s_mgr.camera);
    }
}

// FUN_1018b160, after every PaintScreen: follow the component's race / sex.
void CoA::CharacterCreate::WatchComponent() {
    uint32_t race;
    uint32_t sex;

    if (!RaceSex(race, sex)) {
        // The glue scene (and the preview models in it) is gone; forget them without releasing.
        if (Backend().forgetModels) {
            Backend().forgetModels();
        }

        s_mgr.race.set = false;
        s_mgr.sex.set = false;
        s_mgr.hadComponent = false;
        return;
    }

    if (s_mgr.hadComponent && s_mgr.race.set && s_mgr.race.v == race && s_mgr.sex.set && s_mgr.sex.v == sex) {
        return;
    }

    Set(s_mgr.race, race);
    Set(s_mgr.sex, sex);
    s_mgr.hadComponent = true;
    RebuildLayer0();
    Update();
}

// FUN_1018cad0 (glue screen): drop the preview models and every override; announce what was lost.
void CoA::CharacterCreate::ResetPreview() {
    bool hadSelection = s_mgr.selection[0] || s_mgr.selection[1] || s_mgr.selection[2];
    const Merged& m = s_mgr.merged;
    bool hadPreview = m.spell || m.anim || m.petCreature || m.shapeshift || m.petDisplay || m.weaponsDrawn || !m.items.empty() || m.unk248 ||
        m.hasCharacter || m.hasShape || m.hasCamera || m.hasFacing || m.hasUndress || m.cast || m.petCast;

    if (Backend().releaseModels) {
        Backend().releaseModels();
    }

    s_mgr.mode = 0;
    std::fill(s_mgr.selection, s_mgr.selection + 3, 0u);
    s_mgr.classPreview = 0;
    s_mgr.classPreviewName.clear();

    for (Layer& l : s_mgr.layers) {
        l = Layer();
    }

    s_mgr.merged = Merged();
    std::fill(s_mgr.camera, s_mgr.camera + 3, 0.0f);
    std::fill(s_mgr.zoomOrigin, s_mgr.zoomOrigin + 3, 0.0f);
    s_mgr.race.set = false;
    s_mgr.sex.set = false;
    s_mgr.hadComponent = false;

    if (hadSelection) {
        SelectionChanged();
    }

    if (hadPreview) {
        PreviewChanged();
    }
}

void CoA::CharacterCreate::Reset() {
    s_mgr = Manager();
}

// FUN_1018b290 (and PatchData's 0x6D6).
void CoA::CharacterCreate::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(CoA::CharacterCreate::ResetPreview);
    // SMSG_PATCH_CHARACTER_CREATION_*: struct images; Roles / Categories / Archetypes carry their strings.
    CoA::DbcPatch::Register(0x6D4, "DBFilesClient\\CharacterCreationArchetypeRoles.dbc", 0x5C, 0xFull << 18, OnCreationTablePatched);
    CoA::DbcPatch::Register(0x6D5, "DBFilesClient\\CharacterCreationArchetypeCategories.dbc", 0x64, 0x1Full << 19, OnCreationTablePatched);
    // 0x6D6 (FUN_101df460, registered by the original's PatchData): 0x74-byte image, then 21 C strings.
    CoA::DbcPatch::Register(0x6D6, "DBFilesClient\\CharacterCreationArchetypes.dbc", 0x74, (1ull << 2) | (0xFFFFFull << 8), OnCreationTablePatched);
    CoA::DbcPatch::Register(0x6D7, "DBFilesClient\\CharacterCreationPetDetails.dbc", 0x30, 0, OnCreationTablePatched);
    CoA::DbcPatch::Register(0x6D8, "DBFilesClient\\CharacterCreationShapeshiftDetails.dbc", 0x54, 0, OnCreationTablePatched);
    CoA::DbcPatch::Register(0x6E3, "DBFilesClient\\CharacterCreationArchetypeDetails.dbc", 0x70, 0, OnCreationTablePatched);
    CoA::DbcPatch::Register(0x9CF, "DBFilesClient\\CharacterCreationClassDetails.dbc", 0x70, 0, OnCreationTablePatched);
    COA_EVENT("CHARACTER_CREATE_PREVIEW_CHANGED");
    COA_EVENT("CHARACTER_CREATE_SELECTION_CHANGED");
}
