// AscItemCollections.cpp (see ItemCollections.h): item display collections and the model bookkeeping behind them.
#include "CoA/Source/Modules/ItemCollections.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmData.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    CoA::ItemCollections::Deps s_deps;

    // ---- DLL globals ----------------------------------------------------------------------------------------------
    bool s_applyComponents = true;      // DAT_10bcbda4, SetModelApplyComponents
    bool s_inSetModel = false;          // DAT_10be35e0 (0x5989E0)
    bool s_inTryOn = false;             // DAT_10be35e1 (0x597FC0)
    bool s_slotReAdded = false;         // DAT_10be35e2
    bool s_capture = false;             // DAT_10be357c
    void* s_captured = nullptr;         // DAT_10be3578
    float s_pendingScale = 1.0f;        // DAT_10bcbedc (1.0 in the image; the reconstruction has 0)

    // Model ids: every model 0x81F8F0 creates gets one (FUN_102ca9c0); the counter starts at 1 and advances twice per id.
    uint64_t s_nextModelId = 1;                                                      // DAT_10bcbeb0
    std::unordered_map<void*, uint64_t> s_modelIds;                                  // 0x10BE35A0
    // Attached collection models: component -> item slot -> model ids (0x10BE35C0).
    std::unordered_map<void*, std::unordered_map<uint32_t, std::vector<uint64_t>>> s_attached;
    // displayId -> collection row ids (0x10BE0638). A patched row is read through the table (the original updates the
    // row it points at in place); a new row joins at the next rebuild.
    std::unordered_map<uint32_t, std::vector<uint32_t>> s_collections;
    bool s_collectionsBuilt = false;

    CoA::Dbc::Table& Collections() {
        return CoA::Dbc::Get("DBFilesClient\\ItemDisplayInfoCollections.dbc");
    }

    void BuildCollections() {
        s_collections.clear();
        CoA::Dbc::Table& t = Collections();

        if (t.Loaded()) {
            const uint32_t max = t.MaxId();

            for (uint32_t id = t.MinId(); id <= max; ++id) {
                if (const uint8_t* row = t.Row(id)) {
                    s_collections[CoA::Dbc::Table::U32(row, 4)].push_back(id);
                }

                if (id == 0xFFFFFFFF) {
                    break;
                }
            }
        }

        s_collectionsBuilt = true;
    }

    void AssignModelId(void* model) {   // FUN_102ca9c0
        const uint64_t id = s_nextModelId++;
        s_modelIds[model] = id;
        ++s_nextModelId;
    }

    // The slot folders at 0x10BCBDA8 (one std::string per ITEM_SLOT).
    const char* const kSlotFolders[] = { "Head", "Shoulder", "Collections", "Collections", "Waist", "Collections",
        "Collections", "Collections", "Collections", "", "Cape" };

    const char* Folder(uint32_t index) {
        return index < 11 ? kSlotFolders[index] : "";
    }

    std::string DropExtension(const std::string& s) {   // substr(0, size - 4), the whole name when shorter
        return s.substr(0, s.size() < 4 ? s.size() : s.size() - 4);
    }

    // FUN_102cc980: "Item\ObjectComponents\<folder>\<name>"; per-race models (not for slot 4, with a race, sex not 2)
    // become "...\<name minus extension>_<ChrRaces prefix>_<M|F|>.mdx" ("" without the ChrRaces row).
    std::string ModelPath(uint32_t index, std::string name, uint32_t race, uint32_t sex) {
        if (name.empty()) {
            return std::string();
        }

        if (index == 4 || race == 0 || sex == 2) {
            return std::string("Item\\ObjectComponents\\") + Folder(index) + "\\" + name;
        }

        const char* prefix = s_deps.racePrefix ? s_deps.racePrefix(race) : nullptr;

        if (!prefix) {
            return std::string();
        }

        name = DropExtension(name);
        std::string letter;

        if (sex == 0) {
            letter = "M";
        } else if (sex == 1) {
            letter = "F";
        }

        return std::string("Item\\ObjectComponents\\") + Folder(index) + "\\" + name + "_" + prefix + "_" + letter + ".mdx";
    }

    // FUN_102cc540: "Item\ObjectComponents\<folder>\<name minus extension>.blp".
    std::string TexturePath(uint32_t index, const std::string& name) {
        if (name.empty()) {
            return std::string();
        }

        return std::string("Item\\ObjectComponents") + "\\" + Folder(index) + "\\" + DropExtension(name) + ".blp";
    }

    // FUN_102cba60 (the std::function both removal paths pass): a model belongs to (component, index) when its id is in
    // that list; a match is taken out of the list.
    bool TakeAttached(void* component, uint32_t index, void* model) {
        auto outer = s_attached.find(component);

        if (outer == s_attached.end()) {
            return false;
        }

        auto inner = outer->second.find(index);

        if (inner == outer->second.end()) {
            return false;
        }

        const uint64_t id = CoA::ItemCollections::ModelId(model);

        if (id == 0) {
            return false;
        }

        auto it = std::find(inner->second.begin(), inner->second.end(), id);

        if (it == inner->second.end()) {
            return false;
        }

        inner->second.erase(it);
        return true;
    }

    // FUN_102cb100 on the component's model: its attached models that match are detached (0x8274F0).
    void RemoveAttached(void* component, uint32_t index) {
        void* model = s_deps.componentModel ? s_deps.componentModel(component) : nullptr;

        if (!model || !s_deps.children) {
            return;
        }

        std::vector<void*> children;
        s_deps.children(model, children);
        std::vector<void*> matched;

        for (void* m : children) {
            if (TakeAttached(component, index, m)) {
                matched.push_back(m);
            }
        }

        for (void* m : matched) {
            if (s_deps.detach) {
                s_deps.detach(m);
            }
        }
    }

    // SetModelApplyComponents(on) (0x102CBFC0): lua_toboolean of the first argument.
    int SetModelApplyComponents(lua_State* L) {
        s_applyComponents = lua_toboolean(L, 1) != 0;
        return 0;
    }

    // 0x1020A860: the map is cleared and the table reloaded through FUN_101e7b10. Here the map is rebuilt from the
    // table as patched, in id order.
    int ReloadItemDisplayInfoCollections(lua_State*) {
        BuildCollections();
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "SetModelApplyComponents", SetModelApplyComponents),
        COA_NATIVE("", "ReloadItemDisplayInfoCollections", ReloadItemDisplayInfoCollections),
    };
}

CoA::ItemCollections::Deps& CoA::ItemCollections::Dependencies() {
    return s_deps;
}

int32_t CoA::ItemCollections::SlotIndex(uint32_t invSlot) {   // FUN_102cb240
    switch (invSlot) {
        case 0: return 0;
        case 2: return 1;
        case 3: return 2;
        case 4: return 3;
        case 5: return 4;
        case 6: return 5;
        case 7: return 6;
        case 8: return 7;
        case 9: return 8;
        case 0xE: return 10;
        case 0x12: return 9;
        default: return -1;
    }
}

void CoA::ItemCollections::SetInDressUpSetModel(bool inside) {
    s_inSetModel = inside;
}

void CoA::ItemCollections::SetInDressUpTryOn(bool inside) {
    s_inTryOn = inside;
}

void CoA::ItemCollections::BeforeAddItem() {
    s_slotReAdded = true;
}

// FUN_102cbfe0 (Update3DAttachments): the slot's previous collection models go; then, for a display with collections (a
// race, sex not 2, not inside both dress-up calls, SetModelApplyComponents on), each collection is attached through
// AddLink with its paths, its scale applied by the model hook, and the model recorded.
void CoA::ItemCollections::Attach(void* component, uint32_t itemSlot, const void* displayRec, uint32_t displayId) {
    RemoveAttached(component, itemSlot);
    const uint32_t race = s_deps.componentRace ? s_deps.componentRace(component) : 0;
    const uint32_t sex = s_deps.componentSex ? s_deps.componentSex(component) : 0;

    if (!displayRec || race == 0 || sex == 2 || (s_inSetModel && s_inTryOn) || !s_applyComponents) {
        return;
    }

    if (!s_collectionsBuilt) {
        BuildCollections();
    }

    auto it = s_collections.find(displayId);

    if (it == s_collections.end()) {
        return;
    }

    CoA::Dbc::Table& t = Collections();
    const std::vector<uint32_t> rows = it->second;

    for (uint32_t id : rows) {
        const uint8_t* row = t.Row(id);

        if (!row) {
            continue;
        }

        const std::string model = ModelPath(itemSlot, t.Str(row, 0x10), race, sex);
        std::string texture = TexturePath(itemSlot, t.Str(row, 0x14));

        if (texture.empty()) {
            texture = TexturePath(itemSlot, t.Str(row, 0x10));
        }

        const bool hd = CoA::RealmData::HdPatchLoaded();
        s_pendingScale = CoA::Dbc::Table::F32(row, hd ? 0x1C : 0x18);
        s_capture = true;

        if (s_deps.addLink) {
            s_deps.addLink(s_deps.componentModel ? s_deps.componentModel(component) : nullptr, CoA::Dbc::Table::U32(row, hd ? 0xC : 8),
                model.c_str(), texture.c_str(), displayRec);
        }

        if (void* m = s_captured) {
            if (ModelId(m) == 0) {
                AssignModelId(m);
            }

            s_attached[component][itemSlot].push_back(ModelId(m));
        }

        s_capture = false;
        s_captured = nullptr;
    }
}

// FUN_102cb000 after CM2Scene::CreateModel: the model gets an id and, while a scale is pending, the scaled identity as
// its world transform (then the pending scale is 0); a model created during an attach is captured.
void CoA::ItemCollections::ModelCreated(void* model) {
    if (model) {
        AssignModelId(model);

        if (!(s_pendingScale == 0.0f)) {
            if (s_deps.scaleModel) {
                s_deps.scaleModel(model, s_pendingScale);
            }

            s_pendingScale = 0.0f;
        }
    }

    if (s_capture) {
        s_captured = model;
    }
}

// FUN_102caed0 before CM2Model::Release: the last reference drops the model's id.
void CoA::ItemCollections::ModelReleasing(void* model, uint32_t refCount) {
    if (refCount == 1) {
        s_modelIds.erase(model);
    }
}

void CoA::ItemCollections::BeforeRemoveItemBySlot() {
    s_slotReAdded = false;
}

// FUN_102caaa0 after RemoveItemBySlot: unless the original re-added the slot, its collection models go.
void CoA::ItemCollections::AfterRemoveItemBySlot(void* component, uint32_t invSlot) {
    if (s_slotReAdded) {
        return;
    }

    const int32_t index = SlotIndex(invSlot);

    if (index != -1) {
        RemoveAttached(component, static_cast<uint32_t>(index));
    }
}

uint64_t CoA::ItemCollections::ModelId(void* model) {   // FUN_102cb2e0
    auto it = s_modelIds.find(model);
    return it == s_modelIds.end() ? 0 : it->second;
}

std::vector<uint64_t> CoA::ItemCollections::AttachedIds(void* component, uint32_t itemSlot) {
    auto outer = s_attached.find(component);

    if (outer == s_attached.end()) {
        return {};
    }

    auto inner = outer->second.find(itemSlot);
    return inner == outer->second.end() ? std::vector<uint64_t>() : inner->second;
}

bool CoA::ItemCollections::ApplyComponents() {
    return s_applyComponents;
}

float CoA::ItemCollections::PendingScale() {
    return s_pendingScale;
}

// SMSG_PATCH_ITEM_DISPLAY_INFO_COLLECTIONS (0x56A, FUN_101e1b50): 0x20 bytes {id, displayId, attach, attachHD, -, -,
// scale, scaleHD}, then the model and texture strings. The collections map is not rebuilt.
int32_t CoA::ItemCollections::OnPatchCollection(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t row[8];
    CoA::Read::Data(msg, row, 0x20);
    const std::string model = CoA::Read::Str(msg);
    const std::string texture = CoA::Read::Str(msg);
    CoA::Dbc::Table& t = Collections();
    row[4] = t.AddString(model.c_str());
    row[5] = t.AddString(texture.c_str());
    t.Upsert(row[0], std::vector<uint8_t>(reinterpret_cast<uint8_t*>(row), reinterpret_cast<uint8_t*>(row) + 0x20));
    return 1;
}

void CoA::ItemCollections::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x56A, CoA::ItemCollections::OnPatchCollection);
}

void CoA::ItemCollections::Reset() {
    s_deps = Deps();
    s_applyComponents = true;
    s_inSetModel = false;
    s_inTryOn = false;
    s_slotReAdded = false;
    s_capture = false;
    s_captured = nullptr;
    s_pendingScale = 1.0f;
    s_nextModelId = 1;
    s_modelIds.clear();
    s_attached.clear();
    s_collections.clear();
    s_collectionsBuilt = false;
}
