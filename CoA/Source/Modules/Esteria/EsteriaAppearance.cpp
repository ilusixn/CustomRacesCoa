// Esteria native appearance helpers. Loaded by Wow.exe's permanent import table.
// No WarcraftXL API, loader, events, or runtime patching is used.
#include <windows.h>
#include "CoA/Source/Modules/Esteria/EsteriaCompat.h"
#include "CoA/Source/Modules/EsteriaRaces.h"
using namespace CoA::Esteria::Compat;

#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>
#include <array>
#include <unordered_map>
#include "Engine/Source/Retail/BodyShape.h"
#include "Engine/Source/Retail/RetailLookWire.h"
#include "WoW/Common/ComponentCore/RetailRace.h"
#include <random>
#include <algorithm>

// CoA: our own races stay on 1-65; Esteria's native races live on 66+ and are translated to the ids this DLL and its
// catalogs were built for. Every other race is not Esteria's (127).
unsigned RaceOf(unsigned race)
{
    switch (race)
    {
        case 75: return 68; // Centaur (Alliance; id 75 so the 32-bit race masks wrap to Draenei)
        case 102: return 68; // Centaur (Horde; id 102 wraps to Tauren)
        case 112: return 73; // Hozen (Alliance; id 112 so the 32-bit race masks wrap to Kul Tiran)
        case 110: return 74; // Vorkai (Alliance; id 110 wraps to High Elf)
        case 106: return 75; // Mantid (Horde; id 106 wraps to Blood Elf)
        case 109: return 76; // Ethereal (Alliance; id 109 wraps to Worgen)
        case 90: return 77;  // Banshee (Horde)
        case 87: return 78;  // Faceless One (Horde)
        case 88: return 79;  // Qiraji (Horde)
        case 104: return 65; // Slime Giant (Horde; id 104 wraps to Troll)
        case 101: return 61; // Venthyr (Horde; id 101 so the 32-bit race masks wrap to Undead)
        case 107: return 60; // Kyrian (Alliance; id 107 so the 32-bit race masks wrap to Draenei)
        case 21: return 54; // Naga (Horde): Esteria's Naga model + tables since the Esteria racial port (2026-10-16)
        case 66: return 46; // Highmountain Tauren (Horde)
        case 67: return 47; // Mechagnome (Alliance)
        case 68: return 48; // Earthen Alliance
        case 69: return 49; // Earthen Horde
        case 70: return 51; // Haranir Horde
        case 71: return 50; // Haranir Alliance
        case 74: return 20; // Vulpera (Horde)
        case 82: return 62; // Sethrak (Alliance)
        case 83: return 63; // Gnoll (Horde)
        case 97: return 64; // Saberon (Alliance; id 97 so the 32-bit race masks wrap to Human)
        case 98: return 70; // Drakonid (Horde)
        case 99: return 66; // Drakkin (Alliance)
        case 100: return 69; // Drakonid (Alliance)
        default: return 127;
    }
}
#include <string>
#include <memory>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#include "CoA/Source/Modules/Esteria/HighmountainAppearance.h"
#include "CoA/Source/Modules/Esteria/EarthenAppearance.h"
#include "CoA/Source/Modules/Esteria/HaranirAppearance.h"
#include "CoA/Source/Modules/Esteria/VulperaAppearance.h"
#include "CoA/Source/Modules/Esteria/CreatureAppearance.h"

namespace
{
    bool HighmountainGeometry(void* character);
    bool HighmountainCycle(void* state, char const* command);
    bool PreviewAppearance(void* character, char const* command);
    bool dropdownWheelBlocked = false;
}

struct M2ArrayLayout { std::uint32_t count; std::int32_t offset; };

namespace
{
    constexpr unsigned RaceCount = 129;
    constexpr unsigned AppearanceProfileCount = RaceCount * 2;

    FILE* CatalogFile(wchar_t const* name)
    {
        wchar_t path[MAX_PATH] = {};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
            return nullptr;
        wchar_t* slash = std::wcsrchr(path, L'\\');
        if (!slash || slash - path + 1 + std::wcslen(name) >= MAX_PATH)
            return nullptr;
        std::wcscpy(slash + 1, name);
        FILE* file = nullptr;
        return _wfopen_s(&file, path, L"rb") ? nullptr : file;
    }

    struct Option
    {
        std::uint32_t offset;
        std::uint32_t count;
        std::uint32_t factor;
        std::uint32_t geometryOffset;
        std::uint32_t geometryChoices[32];
    };

    struct Profile
    {
        std::uint32_t race;
        std::uint32_t gender;
        std::uint32_t optionCount;
        Option options[16];
    };

    std::vector<Profile> const& Profiles()
    {
        static std::vector<Profile> const profiles = []
        {
            std::vector<Profile> result;
            FILE* file = CatalogFile(L"EsteriaAppearance.bin");
            if (!file)
                return result;
            std::uint32_t header[3] = {};
            if (std::fread(header, sizeof(header), 1, file) != 1 || header[0] != 0x50504145
                || header[1] != 2 || header[2] > AppearanceProfileCount)
            {
                std::fclose(file);
                return result;
            }
            for (std::uint32_t i = 0; i < header[2]; ++i)
            {
                Profile profile{};
                if (std::fread(&profile, sizeof(profile), 1, file) != 1)
                    break;
                bool valid = profile.race < RaceCount && profile.gender < 2
                    && profile.optionCount > 0 && profile.optionCount <= 16;
                for (std::uint32_t j = 0; valid && j < profile.optionCount; ++j)
                {
                    auto const& option = profile.options[j];
                    valid = (option.offset == 0x24 || option.offset == 0x28 || option.offset == 0x2C
                        || option.offset == 0x30 || option.offset == 0x34)
                        && option.count && option.factor && option.count <= 256
                        && option.factor <= 256 / option.count
                        && (!option.geometryOffset || (option.count <= 32
                            && option.geometryOffset >= 0x144 && option.geometryOffset < 0x190
                            && option.geometryOffset % 4 == 0));
                }
                if (valid)
                    result.push_back(profile);
            }
            std::fclose(file);
            return result;
        }();
        return profiles;
    }

    template<class T>
    T& Field(void* object, std::size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
    }

    struct Geometry
    {
        char model[128];
        std::vector<unsigned char> skin;
    };

    struct MaterialSelector
    {
        unsigned offset;
        unsigned factor;
        unsigned count;
        unsigned value;
    };

    struct AppearanceMaterial
    {
        unsigned race;
        unsigned gender;
        unsigned slot;
        MaterialSelector selectors[3];
        char path[128];
    };

    std::vector<AppearanceMaterial> const& AppearanceMaterials()
    {
        static std::vector<AppearanceMaterial> const materials = []
        {
            std::vector<AppearanceMaterial> result;
            FILE* file = CatalogFile(L"EsteriaAppearanceMaterials.bin");
            if (!file)
                return result;
            unsigned header[3] = {};
            if (std::fread(header, sizeof(header), 1, file) == 1 && header[0] == 0x544D4145
                && header[1] == 1 && header[2] <= 4096)
                for (unsigned i = 0; i < header[2]; ++i)
                {
                    AppearanceMaterial material{};
                    if (std::fread(&material, sizeof(material), 1, file) != 1)
                        break;
                    bool valid = material.race == 47 && material.gender < 2
                        && material.slot == 5 && material.path[0]
                        && std::memchr(material.path, 0, sizeof(material.path));
                    for (auto const& selector : material.selectors)
                        valid = valid && (selector.offset == 0x24 || selector.offset == 0x28
                            || selector.offset == 0x30) && selector.factor && selector.count
                            && selector.count <= 256 && selector.factor <= 256 / selector.count
                            && selector.value < selector.count;
                    if (valid)
                        result.push_back(material);
                }
            std::fclose(file);
            return result;
        }();
        return materials;
    }

    bool ValidGeometry(std::vector<unsigned char>& skin)
    {
        if (skin.size() < 48 || Field<unsigned>(skin.data(), 0) != 0x4E494B53)
            return false;
        unsigned const strides[] = {2, 2, 4, 48, 24};
        for (unsigned i = 0; i < 5; ++i)
        {
            unsigned count = Field<unsigned>(skin.data(), 4 + i * 8);
            unsigned offset = Field<unsigned>(skin.data(), 8 + i * 8);
            if (offset > skin.size() || count > (skin.size() - offset) / strides[i])
                return false;
        }
        unsigned vertices = Field<unsigned>(skin.data(), 4);
        unsigned indices = Field<unsigned>(skin.data(), 12);
        unsigned meshes = Field<unsigned>(skin.data(), 28);
        if (!vertices || vertices > 65535 || !indices || !meshes || meshes > 4096)
            return false;
        auto* sections = skin.data() + Field<unsigned>(skin.data(), 32);
        for (unsigned i = 0; i < meshes; ++i)
        {
            auto* section = sections + i * 48;
            unsigned start = Field<std::uint16_t>(section, 8)
                | (Field<std::uint16_t>(section, 2) << 16);
            if (start > indices || Field<std::uint16_t>(section, 10) > indices - start
                || unsigned(Field<std::uint16_t>(section, 4)) + Field<std::uint16_t>(section, 6) > vertices
                || Field<std::uint16_t>(section, 12) > 75)
                return false;
        }
        return true;
    }

    std::vector<Geometry>& Geometries()
    {
        static std::vector<Geometry> geometries = []
        {
            std::vector<Geometry> result;
            FILE* file = CatalogFile(L"EsteriaAppearanceGeometry.bin");
            if (!file)
                return result;
            unsigned header[3] = {};
            if (std::fread(header, sizeof(header), 1, file) == 1 && header[0] == 0x4D474145
                && header[1] == 1 && header[2] <= AppearanceProfileCount)
                for (unsigned i = 0; i < header[2]; ++i)
                {
                    Geometry geometry{};
                    unsigned length = 0;
                    if (std::fread(geometry.model, sizeof(geometry.model), 1, file) != 1
                        || !std::memchr(geometry.model, 0, sizeof(geometry.model))
                        || std::fread(&length, sizeof(length), 1, file) != 1 || length > 16 * 1024 * 1024)
                        break;
                    geometry.skin.resize(length);
                    if (std::fread(geometry.skin.data(), length, 1, file) != 1 || !ValidGeometry(geometry.skin))
                        break;
                    result.push_back(std::move(geometry));
                }
            std::fclose(file);
            return result;
        }();
        return geometries;
    }

    std::uintptr_t Address(std::uintptr_t preferred)
    {
        return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + preferred - 0x00400000;
    }

    template<class T>
    T Native(std::uintptr_t preferred)
    {
        return reinterpret_cast<T>(Address(preferred));
    }

    void Refresh(void* character)
    {
        // These are the setters the stock component initializer itself calls.
        N_4EA2F0(
            character, CC<unsigned>(character, 0x24), 0, 0);
        N_4EA6B0(
            character, CC<unsigned>(character, 0x28), 0, 1, 0);
        N_4EA490(
            character, CC<unsigned>(character, 0x2C), 0, 0);
        N_4EA3E0(
            character, CC<unsigned>(character, 0x34), 0);
        N_4EA590(
            character, CC<unsigned>(character, 0x30), 0, 0);
        SetPrevSkinIndex(CC<unsigned>(character, 0x28));
        N_4E6AE0(character, 1);
    }
}

#include "CosmeticWings.inl"

void EsteriaSkin(void* modelPointer)
{
    auto* model = static_cast<CM2Shared*>(modelPointer);
    if (!model)
        return;
    M2SkinProfile* skin = model->m_skinData;
    if (!skin)
        return;
    for (auto& geometry : Geometries())
    {
        if (_stricmp(model->m_fileName, geometry.model))
            continue;
        // Restore the prepared Wrath arrays if another asset reader parked wide-index meshes.
        // The engine still owns and frees its original profile block. The x64 arrays are self-relative 32-bit offsets, so
        // the catalog copy has to live within +-2 GB of the profile (one block per profile, kept until exit).
        // Keyed by profile and geometry: a freed profile's address can come back for another model.
        static std::unordered_map<void*, std::pair<void const*, unsigned char*>> blocks;
        auto& cached = blocks[skin];
        if (cached.first != &geometry || !cached.second)
        {
            auto* fresh = static_cast<unsigned char*>(AllocNear(skin, geometry.skin.size()));
            if (!fresh)
                return;
            std::memcpy(fresh, geometry.skin.data(), geometry.skin.size());
            cached = {&geometry, fresh};
        }
        unsigned char* block = cached.second;
        M2ArrayLayout* arrays[5] = {reinterpret_cast<M2ArrayLayout*>(&skin->m_vertices), reinterpret_cast<M2ArrayLayout*>(&skin->m_indices),
            reinterpret_cast<M2ArrayLayout*>(&skin->m_bones), reinterpret_cast<M2ArrayLayout*>(&skin->m_skinSections),
            reinterpret_cast<M2ArrayLayout*>(&skin->m_batches)};
        for (unsigned i = 0; i < 5; ++i)
        {
            arrays[i]->count = Field<unsigned>(geometry.skin.data(), 4 + i * 8);
            arrays[i]->offset = static_cast<std::int32_t>(block + Field<unsigned>(geometry.skin.data(), 8 + i * 8)
                - reinterpret_cast<unsigned char*>(arrays[i]));
        }
        skin->m_boneCountMax = 75;
        return;
    }
}

void EsteriaGeometry(void* character)
{
    if (!character)
        return;
    UpdateCosmeticWing(character);
    // CoA: boots swap the shin/foot geoset group for numbers Mechagnome (67) and Naga (21) do not have, so the legs vanish
    // (Naga: a boot piece lands on the head). Hide the feet slot display for them so nothing is applied.
    if (CC<unsigned>(character, 0x18) == 67 || CC<unsigned>(character, 0x18) == 21)
        CC<unsigned>(character, 0x428 + 6 * 4) = 0;
    if (HighmountainGeometry(character))
        return;
    for (auto const& profile : Profiles())
    {
        if (profile.race != RaceOf(CC<unsigned>(character, 0x18))
            || profile.gender != CC<unsigned>(character, 0x1C))
            continue;
        for (unsigned i = 0; i < profile.optionCount; ++i)
        {
            auto const& option = profile.options[i];
            if (option.geometryOffset)
            {
                unsigned choice = (CC<unsigned>(character, option.offset) / option.factor) % option.count;
                CC<unsigned>(character, option.geometryOffset) = option.geometryChoices[choice];
            }
        }
        if (profile.race == 47 && profile.optionCount == 12)
        {
            auto choice = [&](unsigned index)
            {
                auto const& option = profile.options[index];
                return (CC<unsigned>(character, option.offset) / option.factor) % option.count;
            };
            unsigned modification = choice(7);
            unsigned visor = modification / 4;
            CC<unsigned>(character, 0x148) = choice(4) ? 101 + choice(4) : 100;
            // CoA: the client shadow pass chokes on this model above ~70k triangle indices, so only the default body (301) and
            // leg (501/1001) styles exist in the shipped skin; the other choices would hide the arms or legs.
            CC<unsigned>(character, 0x150) = 301;
            CC<unsigned>(character, 0x158) = 501;
            CC<unsigned>(character, 0x16C) = 1001;
            CC<unsigned>(character, 0x160) = 702 + modification % 4;
            CC<unsigned>(character, 0x184) = visor ? 1601 + (visor == 4 ? 2 : visor) : 1600;
            CC<unsigned>(character, 0x18C) = visor == 2 ? 1803 : 1800;
            unsigned eye = choice(8);
            unsigned primal = profile.options[8].count - 4;
            CC<unsigned>(character, 0x188) = eye >= primal ? 1702 + eye - primal : eye == 14 ? 1701 : 1700;
            void* instance = CC<void*>(character, 0x38);
            if (instance)
            {
                unsigned boundSlots = 0;
                for (auto const& material : AppearanceMaterials())
                {
                    if (material.gender != profile.gender || (boundSlots & (1u << material.slot)))
                        continue;
                    bool selected = true;
                    for (auto const& selector : material.selectors)
                        selected = selected && (CC<unsigned>(character, selector.offset)
                            / selector.factor) % selector.count == selector.value;
                    if (!selected)
                        continue;
                    // Same load/set/release sequence used by the native extra-head setter.
                    void* texture = N_4E8D30(
                        material.path, nullptr);
                    if (texture)
                    {
                        N_825260(
                            instance, material.slot, texture);
                        N_47BF30(texture);
                    }
                    boundSlots |= 1u << material.slot;
                }
            }
        }
        if (profile.optionCount > 7 && profile.options[2].geometryOffset == 0x184)
        {
            auto const& enabled = profile.options[6];
            auto const& color = profile.options[7];
            unsigned feather = CC<unsigned>(character, 0x184);
            unsigned toggle = (CC<unsigned>(character, enabled.offset) / enabled.factor) % enabled.count;
            unsigned palette = (CC<unsigned>(character, color.offset) / color.factor) % color.count;
            CC<unsigned>(character, 0x184) = feather && toggle ? feather + palette * 100 : 1600;
            void* instance = CC<void*>(character, 0x38);
            if (instance)
                N_82C7C0(instance, 4000, 4799, 0);
        }
        break;
    }
}

bool EsteriaDropdownWheelBlocked()
{
    return dropdownWheelBlocked;
}

namespace
{
    // The bounding boxes of the retail-built models do not report their real size, so the big races have a set stage scale.
    float FixedPreviewScale(unsigned race)
    {
        switch (race)
        {
        case 54: case 58: return .5f;   // Dracthyr
        case 107: return .6f;           // Kyrian
        case 101: return .7f;           // Venthyr
        case 75: case 102: return .55f; // Centaur
        case 87: return .7f;            // Faceless One
        default: return 0.f;
        }
    }

    // Fits a preview model on the creation / selection screen: Vrykul get their fixed size, anything else whose bounding box
    // (height, or 0.8 x the wider side for wings and centaur bodies) passes 2.2 units is shrunk uniformly to 2.2.
    void ScaleVrykulPreview(void* character)
    {
        if (!character)
            return;
        void* instance = CC<void*>(character, 0x38);
        CM2Shared* model = instance ? ModelShared(instance) : nullptr;
        float const fixedScale = FixedPreviewScale(RaceOf(CC<unsigned>(character, 0x18)));
        if (!instance || (!model && !fixedScale))
            return;
        auto const* matrix = ModelMatrix(instance);
        float current = std::hypot(matrix[0], matrix[1]);
        float position[] = {matrix[12], matrix[13], matrix[14]};
        if (!std::isfinite(current) || current < .001f
            || !std::all_of(std::begin(position), std::end(position), [](float v) { return std::isfinite(v); }))
            return;
        float target = 0.f;
        if (fixedScale)
            target = fixedScale;
        else if ((RaceOf(CC<unsigned>(character, 0x18)) == 56 || RaceOf(CC<unsigned>(character, 0x18)) == 57)
            && !_strnicmp(model->m_fileName, "custom\\vrykul\\", 14))
            target = .55f;
        else
        {
            auto const& box = model->m_boundingBox;
            float const height = box.t.z - box.b.z;
            float const width = std::max(box.t.x - box.b.x, box.t.y - box.b.y);
            float const extent = std::max(height, .8f * width);
            if (!std::isfinite(extent) || extent <= 2.2f)
                return;
            target = std::max(.25f, 2.2f / extent);
        }
        if (std::abs(current - target) < .0001f)
            return;
        // Verified 12340 UI placement ABI: position, yaw, uniform scale. Preserve facing and pedestal position.
        N_8251D0(instance, position, std::atan2(matrix[1], matrix[0]), target);
        char line[96];
        std::snprintf(line, sizeof(line), "preview scale: race %u %.3f -> %.3f", RaceOf(CC<unsigned>(character, 0x18)), current, target);
        BodyShape::Log(line);
    }
}

int EsteriaCycle(void* state)
{
    auto const isNumber = N_84DF20;
    if (isNumber(state, 1))
        return 0; // a numeric first argument is the stock CycleCharCustomization, which the caller runs
    auto const getTop = N_84DBD0;
    auto const number = N_84E030;
    auto const string = N_84E0E0;
    auto const pushNumber = N_84E2A0;
    char const* command = string(state, 1, nullptr);
    if (!command || getTop(state) < 2 || !isNumber(state, 2))
        return 0;
    if (std::strcmp(command, "EA_WHEEL_BLOCK") == 0)
    {
        double active = number(state, 2);
        if (active == 0 || active == 1)
            dropdownWheelBlocked = active == 1;
        return 0;
    }
    if (std::strcmp(command, "EA_SELECT") == 0)
    {
        double index = number(state, 2);
        unsigned count = SelCount();
        if (count > 100 || !std::isfinite(index) || index < 1 || index > count
            || index != std::floor(index))
            return 0;
        auto* row = SelRow(static_cast<unsigned>(index) - 1);
        pushNumber(state, row[0x178]);
        pushNumber(state, row[0x17A]);
        return 2;
    }
    if (std::strcmp(command, "EA_BODY_SAVE") == 0)
    {
        CoA::Esteria::BodySliders::SavePending();
        return 0;
    }
    if (std::strcmp(command, "EA_MODEL_HEIGHT") == 0)
    {
        // Height of the character-create preview model in world units (bounding box x instance scale): the creator zooms by it.
        void* previewCharacter = CreationCharacter();
        void* previewInstance = previewCharacter ? CC<void*>(previewCharacter, 0x38) : nullptr;
        float height = 0.f;
        if (previewInstance)
            if (CM2Shared* shared = ModelShared(previewInstance))
            {
                auto const* matrix = ModelMatrix(previewInstance);
                float scale = std::hypot(matrix[0], matrix[1]);
                height = (shared->m_boundingBox.t.z - shared->m_boundingBox.b.z) * scale;
            }
        pushNumber(state, std::isfinite(height) ? height : 0.f);
        return 1;
    }
    if (std::strcmp(command, "EA_PREVIEW_SCALE") == 0)
    {
        double index = number(state, 2);
        if (!std::isfinite(index) || index < 0 || index > 100 || index != std::floor(index))
            return 0;
        if (index == 0)
            ScaleVrykulPreview(CreationCharacter());
        else
        {
            unsigned count = SelCount();
            if (count <= 100 && index <= count)
                ScaleVrykulPreview(Field<void*>(SelRow(static_cast<unsigned>(index) - 1), 0x188));
        }
        return 0;
    }
    void* character = CreationCharacter();
    if (!character)
        return 0;
    {
        // Races built from the Retail files answer the EA_* commands themselves (options, forms, preview); -1: not one.
        int const retail = RetailRaceCycleCommand(static_cast<lua_State*>(state), static_cast<CCharacterComponent*>(character), command);
        if (retail >= 0)
            return retail;
    }
    if (std::strcmp(command, "EA_PREVIEW_SAVE") == 0 || std::strcmp(command, "EA_PREVIEW_RESTORE") == 0
        || std::strcmp(command, "EA_PREVIEW_END") == 0)
    {
        pushNumber(state, PreviewAppearance(character, command) ? 1 : 0);
        return 1;
    }
    if (std::strcmp(command, "EA_SET") == 0 && (getTop(state) != 3 || !isNumber(state, 3)))
        return 0;
    if (std::strcmp(command, "EA_STOCK_GET") == 0 || std::strcmp(command, "EA_STOCK_CHOICES") == 0)
    {
        double requested = number(state, 2);
        unsigned race = CC<unsigned>(character, 0x18);
        unsigned gender = CC<unsigned>(character, 0x1C);
        if (!std::isfinite(requested) || requested < 1 || requested > 5
            || requested != std::floor(requested) || race >= RaceCount || gender > 1)
            return 0;
        unsigned index = static_cast<unsigned>(requested) - 1;
        constexpr unsigned offsets[] = {0x28, 0x2C, 0x34, 0x24, 0x30};
        constexpr unsigned categories[] = {0, 1, 3, 3, 2};
        void* table = ChrVarTable();
        if (!table)
            return 0;
        auto variations = N_4F3AE0;
        auto colors = N_4F3B10;
        auto section = N_4F3BA0;
        unsigned flags = N_4F3A40(
            0, CC<unsigned>(character, 0x20));
        auto allowed = [&](unsigned category, unsigned variation, unsigned color)
        {
            auto row = section(table, race, gender, category, variation, color, nullptr);
            return row && N_4F39A0(row->m_flags, flags);
        };
        unsigned category = categories[index];
        unsigned style = CC<unsigned>(character, 0x34);
        bool texturedFeature = false;
        if (index == 4)
            section(table, race, gender, 2, CC<unsigned>(character, 0x30),
                CC<unsigned>(character, 0x24), &texturedFeature);
        auto featureCounts = FacialHairCounts();
        unsigned count = index == 0 ? colors(table, race, gender, 0, 0)
            : index == 3 ? colors(table, race, gender, 3, style)
            : index == 4 && !texturedFeature ? (featureCounts ? featureCounts[race * 2 + gender] : 0)
            : variations(table, race, gender, category);
        if (count > 256)
            return 0;
        if (std::strcmp(command, "EA_STOCK_GET") == 0)
        {
            pushNumber(state, CC<unsigned>(character, offsets[index]));
            pushNumber(state, count);
            return 2;
        }
        std::string choices;
        for (unsigned value = 0; value < count; ++value)
        {
            bool valid = index == 4 && !texturedFeature;
            if (index == 0)
                valid = allowed(0, 0, value) && allowed(1, CC<unsigned>(character, 0x2C), value);
            else if (index == 3)
                valid = allowed(3, style, value);
            else if (!valid)
                for (unsigned color = 0, total = (std::min)(colors(table, race, gender, category, value), 256u);
                    color < total && !valid; ++color)
                    valid = allowed(category, value, color);
            if (valid)
                choices += std::to_string(value) + ",";
        }
        N_84E350(state, choices.c_str());
        return 1;
    }
    if (HighmountainCycle(state, command))
        return std::strcmp(command, "EA_GET") == 0 ? 3 : std::strcmp(command, "EA_CHOICES") == 0 ? 1 : 0;
    Profile const* profile = nullptr;
    for (auto const& candidate : Profiles())
        if (candidate.race == RaceOf(CC<unsigned>(character, 0x18))
            && candidate.gender == CC<unsigned>(character, 0x1C))
        {
            profile = &candidate;
            break;
        }
    double requested = number(state, 2);
    if (!profile || !std::isfinite(requested) || requested < 1 || requested > profile->optionCount
        || requested != std::floor(requested))
        return 0;
    if (std::strcmp(command, "EA_RANDOM") == 0)
    {
        // Profile races (Mechagnome ...) had no random handler: roll every option, keeping each packed byte <= 255.
        static std::mt19937 profileRandom(GetTickCount() ^ 0x5bd1e995u);
        for (unsigned i = 0; i < profile->optionCount; ++i)
        {
            auto const& each = profile->options[i];
            unsigned& field = CC<unsigned>(character, each.offset);
            unsigned current = (field / each.factor) % each.count;
            unsigned pick = std::uniform_int_distribution<unsigned>(0, each.count - 1)(profileRandom);
            unsigned replacement = field - current * each.factor + pick * each.factor;
            if (replacement <= 255)
                field = replacement;
        }
        Refresh(character);
        return 0;
    }
    auto const& option = profile->options[static_cast<unsigned>(requested) - 1];
    unsigned& encoded = CC<unsigned>(character, option.offset);
    unsigned const value = (encoded / option.factor) % option.count;
    if (std::strcmp(command, "EA_GET") == 0)
    {
        pushNumber(state, value);
        pushNumber(state, option.count);
        return 2;
    }
    if (std::strcmp(command, "EA_CHOICES") == 0)
    {
        std::string choices;
        for (unsigned i = 0; i < option.count; ++i)
            choices += std::to_string(i) + ",";
        N_84E350(state, choices.c_str());
        return 1;
    }
    bool direct = std::strcmp(command, "EA_SET") == 0;
    if ((!direct && std::strcmp(command, "EA_CYCLE") != 0) || getTop(state) != 3 || !isNumber(state, 3))
        return 0;
    double delta = number(state, 3);
    if (direct ? (!std::isfinite(delta) || delta < 0 || delta >= option.count || delta != std::floor(delta))
        : (delta != 1 && delta != -1))
        return 0;
    unsigned const next = direct ? static_cast<unsigned>(delta)
        : (value + option.count + static_cast<int>(delta)) % option.count;
    unsigned const replacement = encoded - value * option.factor + next * option.factor;
    if (replacement > 255)
        return 0;
    encoded = replacement;
    Refresh(character);
    return 0;
}

unsigned EsteriaSkillRace(unsigned race)
{
    // Match the server's legacy eligibility masks without changing character identity.
    if (RaceOf(race) == 127)
        return race;
    if (race == 75)
        return 11;
    if (race == 102)
        return 6;
    if (race == 112)
        return 16;
    if (race == 104)
        return 8;
    if (race == 97)
        return 1; // CoA: Saberon is Alliance here (Esteria's slot 64 is Horde -> Orc masks, NPCs looked hostile)
    switch (RaceOf(race))
    {
        case 45: return 2;
        case 46: return 6;
        case 47: return 7;
        case 48: return 3;
        case 49: return 2;
        case 50: return 4;
        case 51: return 8;
        case 52: return 13;
        case 53: return 10;
        case 54: return 10;
        case 55: return 3;
        case 56:
        case 58: return 1;
        case 57:
        case 59: return 2;
        case 60: return 11; // Kyrian: Draenei skill lines
        case 61: return 5; // Venthyr: Undead skill lines
        case 62: return 1;
        case 63: return 2;
        case 64: return 2;
        case 65: return 1;
        default: return race;
    }
}

std::uint32_t EsteriaWideSource(void const* section, void const* skin)
{
    if (!section)
        return 0;
    auto const* data = static_cast<unsigned char const*>(section);
    std::uint32_t const low = *reinterpret_cast<std::uint16_t const*>(data + 8);
    std::uint32_t const high = *reinterpret_cast<std::uint16_t const*>(data + 2);
    std::uint32_t const count = *reinterpret_cast<std::uint16_t const*>(data + 10);
    std::uint32_t const wide = (high << 16) | low;
    if (skin)
    {
        std::uint32_t const total = *reinterpret_cast<std::uint32_t const*>(
            static_cast<unsigned char const*>(skin) + 0x0C);
        if (wide <= total && count <= total - wide)
            return wide;
    }
    return low;
}

std::uint32_t EsteriaDrawStart(void const* section, void* instance)
{
    CM2Shared* model = instance ? ModelShared(instance) : nullptr;
    return EsteriaWideSource(section, model ? model->m_skinData : nullptr);
}

namespace
{
    bool ExtendedRace(unsigned race)
    {
        return race == 20 || race == 46 || race == 48 || race == 49 || race == 50 || race == 51
            || CreatureAppearance::Uses(race);
    }

    bool ValidateExtended(unsigned race, unsigned gender, std::array<std::uint8_t, 13> const& fields)
    {
        if (CreatureAppearance::Uses(race))
        {
            std::array<std::uint8_t, 5> appearance{};
            std::copy_n(fields.begin(), appearance.size(), appearance.begin());
            // Renderer fields are logical; validate the corresponding wire layout (Aetheria 20261027 Kyrian update).
            if (race == 60)
                appearance = CreatureAppearance::EncodeKyrian(gender, appearance);
            return std::all_of(fields.begin() + 5, fields.end(), [](std::uint8_t value) { return value == 0; })
                && CreatureAppearance::Validate(race, gender, appearance);
        }
        if (race == 20)
        {
            std::array<std::uint8_t, 5> vulpera{};
            std::copy_n(fields.begin(), vulpera.size(), vulpera.begin());
            return std::all_of(fields.begin() + 5, fields.end(), [](std::uint8_t value) { return value == 0; })
                && VulperaAppearance::Validate(gender, vulpera);
        }
        if (race == 50 || race == 51)
            return HaranirAppearance::Validate(gender, fields);
        std::array<std::uint8_t, 6> legacy{};
        std::copy_n(fields.begin(), legacy.size(), legacy.begin());
        return std::all_of(fields.begin() + 6, fields.end(), [](std::uint8_t value) { return value == 0; })
            && (race == 46 ? HighmountainAppearance::Validate(gender, legacy)
                : EarthenAppearance::Validate(gender, legacy));
    }

    std::unordered_map<void*, std::array<std::uint8_t, 13>> earthenNetworkAppearance;

    void CaptureEarthenAppearance(void* character, unsigned char const* network, std::uint64_t extra)
    {
        unsigned race = RaceOf(CC<unsigned>(character, 0x18));
        if ((race != 20 && race != 48 && race != 49 && race != 50 && race != 51
            && !CreatureAppearance::Uses(race)) || !network)
            return;
        std::array<std::uint8_t, 13> values{};
        std::memcpy(values.data(), network, 5);
        for (unsigned i = 0; i < 8; ++i)
            values[5 + i] = race == 20 || CreatureAppearance::Uses(race) ? 0
                : static_cast<std::uint8_t>(extra >> (i * 8));
        if (race == 60) // network bytes are the wire layout; the renderer works on logical fields
            values = CreatureAppearance::DecodeKyrian(CC<unsigned>(character, 0x1C), values);
        if (!ValidateExtended(race, CC<unsigned>(character, 0x1C), values))
            return;
        earthenNetworkAppearance[character] = values;
    }

    std::uint64_t UnitAppearanceExtra(void* unit)
    {
        auto* fields = UnitFields(unit);
        if (!fields)
            return 0;
        std::uint64_t extra = Field<unsigned>(fields, 0x8D * 4);
        void* character = UnitComponent(unit);
        if (UnitTypeId(unit) == 4 && character
            && (RaceOf(CC<unsigned>(character, 0x18)) == 50 || RaceOf(CC<unsigned>(character, 0x18)) == 51))
        {
            auto* objectFields = UnitObjectFields(unit);
            if (objectFields)
                extra |= std::uint64_t{Field<unsigned>(objectFields, 5 * 4)} << 32;
        }
        return extra;
    }

    struct OwnerSearch { void* character; void* unit; };

    int32_t FindOwner(WOWGUID guid, void* param)
    {
        auto* search = static_cast<OwnerSearch*>(param);
        void* unit = ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__);
        if (unit && UnitComponent(unit) == search->character)
        {
            search->unit = unit;
            return 0;
        }
        return 1;
    }

    void CaptureEarthenOwner(void* character)
    {
        // ponytail: bounded owner lookup during dirty component rebuilds; index by GUID if profiling warrants it.
        if (!ClntObjMgrGetCurrent()) // no object manager before the world is entered (character select / create)
            return;
        OwnerSearch search{character, nullptr};
        ClntObjMgrEnumVisibleObjects(FindOwner, &search);
        void* unit = search.unit;
        if (!unit)
            return;
        auto* playerData = PlayerData(unit);
        auto* fields = UnitFields(unit);
        if (playerData && fields)
            CaptureEarthenAppearance(character, playerData + 0x14, UnitAppearanceExtra(unit));
    }

    std::unordered_map<void*, std::uint64_t> highmountainExtra;
    std::unordered_map<std::uint64_t, std::uint64_t> highmountainRoster;
    thread_local void* highmountainContext = nullptr;
    constexpr unsigned appearanceOffsets[] = {0x28, 0x2C, 0x34, 0x24, 0x30};

    std::array<std::uint8_t, 13> HighmountainFields(void* character)
    {
        std::array<std::uint8_t, 13> result{};
        for (unsigned i = 0; i < 5; ++i)
            result[i] = static_cast<std::uint8_t>(CC<unsigned>(character, appearanceOffsets[i]));
        auto it = highmountainExtra.find(character);
        std::uint64_t extra = it == highmountainExtra.end() ? 0 : it->second;
        for (unsigned i = 0; i < 8; ++i)
            result[5 + i] = RaceOf(CC<unsigned>(character, 0x18)) == 20
                || CreatureAppearance::Uses(RaceOf(CC<unsigned>(character, 0x18))) ? 0
                : static_cast<std::uint8_t>(extra >> (i * 8));
        if (RaceOf(CC<unsigned>(character, 0x18)) == 60)
            result = CreatureAppearance::DecodeKyrian(CC<unsigned>(character, 0x1C), result);
        return result;
    }

    void SetHighmountainFields(void* character, std::array<std::uint8_t, 13> const& input)
    {
        auto values = RaceOf(CC<unsigned>(character, 0x18)) == 60
            ? CreatureAppearance::EncodeKyrian(CC<unsigned>(character, 0x1C), input) : input;
        for (unsigned i = 0; i < 5; ++i)
            CC<unsigned>(character, appearanceOffsets[i]) = values[i];
        highmountainExtra[character] = HaranirAppearance::Extra(values);
    }

    bool PreviewAppearance(void* character, char const* command)
    {
        // Character Create owns one model; keep its complete packed appearance for transient hovers.
        static void* owner = nullptr;
        static unsigned race = 0, gender = 0, characterClass = 0;
        static std::array<std::uint8_t, 13> saved{};
        if (std::strcmp(command, "EA_PREVIEW_END") == 0)
        {
            owner = nullptr;
            return true;
        }
        if (std::strcmp(command, "EA_PREVIEW_SAVE") == 0)
        {
            owner = character;
            race = RaceOf(CC<unsigned>(character, 0x18));
            gender = CC<unsigned>(character, 0x1C);
            characterClass = CC<unsigned>(character, 0x20);
            saved = HighmountainFields(character);
            return true;
        }
        if (owner != character || race != RaceOf(CC<unsigned>(character, 0x18))
            || gender != CC<unsigned>(character, 0x1C) || characterClass != CC<unsigned>(character, 0x20))
            return false;
        SetHighmountainFields(character, saved);
        Refresh(character);
        return true;
    }

    // Explicit descriptor/rule tables come from the pinned Retail choices; version-2 profiles stay intact.
    template<class Options, class Labels, class Requirements>
    bool HighmountainControl(void* state, char const* command, void* character, Options const& options,
        Labels const& labels, Requirements const& requirements)
    {
        auto const number = N_84E030;
        auto const pushNumber = N_84E2A0;
        auto const pushString = N_84E350;
        double requested = number(state, 2);
        if (!std::isfinite(requested) || requested < 1 || requested > options.size()
            || requested != std::floor(requested))
        {
            if (std::strcmp(command, "EA_GET") == 0)
            {
                pushNumber(state, 0);
                pushNumber(state, 1);
                pushString(state, "");
            }
            else if (std::strcmp(command, "EA_CHOICES") == 0)
                pushString(state, "");
            return true;
        }
        unsigned index = static_cast<unsigned>(requested) - 1;
        auto fields = HighmountainFields(character);
        unsigned race = RaceOf(CC<unsigned>(character, 0x18));
        unsigned gender = CC<unsigned>(character, 0x1C);
        unsigned characterClass = CC<unsigned>(character, 0x20);
        auto normalize = [&](std::array<std::uint8_t, 13>& values)
        {
            if (race != 20)
                return;
            std::array<std::uint8_t, 5> vulpera{};
            std::copy_n(values.begin(), vulpera.size(), vulpera.begin());
            vulpera = VulperaAppearance::Normalize(gender, characterClass, vulpera);
            std::copy(vulpera.begin(), vulpera.end(), values.begin());
        };
        auto allowed = [&](std::array<std::uint8_t, 13> const& values)
        {
            if (!ValidateExtended(race, gender, values))
                return false;
            if (race != 20)
                return true;
            std::array<std::uint8_t, 5> vulpera{};
            std::copy_n(values.begin(), vulpera.size(), vulpera.begin());
            return VulperaAppearance::ValidateClass(gender, characterClass, vulpera);
        };
        auto original = fields;
        normalize(fields);
        if (fields != original)
        {
            SetHighmountainFields(character, fields);
            Refresh(character);
        }
        if (std::strcmp(command, "EA_RANDOM") == 0)
        {
            static std::mt19937 random(GetTickCount());
            fields = {};
            normalize(fields);
            for (unsigned i = 0; i < options.size(); ++i)
            {
                auto const& descriptor = options[i];
                unsigned first = std::uniform_int_distribution<unsigned>(0, descriptor.count - 1)(random);
                for (unsigned attempt = 0; attempt < descriptor.count; ++attempt)
                {
                    auto candidate = fields;
                    unsigned value = (first + attempt) % descriptor.count;
                    unsigned prior = candidate[descriptor.field] / descriptor.factor % descriptor.count;
                    candidate[descriptor.field] = static_cast<std::uint8_t>(
                        candidate[descriptor.field] - prior * descriptor.factor + value * descriptor.factor);
                    if (allowed(candidate))
                    {
                        fields = candidate;
                        break;
                    }
                }
            }
            SetHighmountainFields(character, fields);
            Refresh(character);
            return true;
        }
        auto const& option = options[index];
        unsigned value = (fields[option.field] / option.factor) % option.count;
        if (std::strcmp(command, "EA_GET") == 0)
        {
            pushNumber(state, value);
            unsigned count = option.count;
            if (race == 20 && index == 9)
            {
                auto candidate = fields;
                candidate[option.field] = static_cast<std::uint8_t>(candidate[option.field] - value * option.factor
                    + option.factor);
                if (!allowed(candidate))
                    count = 1;
            }
            pushNumber(state, count);
            pushString(state, labels[index]);
            return true;
        }
        bool choices = std::strcmp(command, "EA_CHOICES") == 0;
        bool direct = std::strcmp(command, "EA_SET") == 0;
        if (!choices && !direct && std::strcmp(command, "EA_CYCLE") != 0)
            return true;
        double direction = choices ? 1 : number(state, 3);
        if (direct ? (!std::isfinite(direction) || direction < 0 || direction >= option.count
            || direction != std::floor(direction)) : (direction != 1 && direction != -1))
            return true;
        std::string available;
        for (unsigned step = 1; step <= (direct ? 1u : option.count); ++step)
        {
            unsigned next = choices ? step - 1 : direct ? static_cast<unsigned>(direction)
                : static_cast<unsigned>((static_cast<int>(value) + static_cast<int>(option.count) * 2
                    + static_cast<int>(step) * static_cast<int>(direction)) % static_cast<int>(option.count));
            auto candidate = fields;
            candidate[option.field] = static_cast<std::uint8_t>(
                candidate[option.field] - value * option.factor + next * option.factor);
            // Changing a prerequisite resets incompatible dependent accessories to their authored default.
            for (unsigned pass = 0; pass < options.size(); ++pass)
                for (auto const& rule : requirements)
                {
                    auto const& selected = options[rule.option];
                    auto const& required = options[rule.required];
                    unsigned dependent = (candidate[selected.field] / selected.factor) % selected.count;
                    unsigned prerequisite = (candidate[required.field] / required.factor) % required.count;
                    if (rule.option != index && dependent == rule.value
                        && !(rule.mask & (std::uint64_t{1} << prerequisite)))
                        candidate[selected.field] -= static_cast<std::uint8_t>(dependent * selected.factor);
                }
            if (!allowed(candidate))
                continue;
            if (choices)
            {
                available += std::to_string(next) + ",";
                continue;
            }
            SetHighmountainFields(character, candidate);
            Refresh(character);
            break;
        }
        if (choices)
            pushString(state, available.c_str());
        return true;
    }

    bool HighmountainCycle(void* state, char const* command)
    {
        void* character = CreationCharacter();
        if (!character || !ExtendedRace(RaceOf(CC<unsigned>(character, 0x18))))
            return false;
        if (RaceOf(CC<unsigned>(character, 0x18)) == 54)
        {
            constexpr std::array<char const*, 5> maleLabels =
                {"Skin Color", "Face", "Crest Style", "Eye Color", "Facial Features"};
            constexpr std::array<char const*, 5> femaleLabels =
                {"Skin Color", "Face", "Hair Style", "Hair Color", "Facial Features"};
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            unsigned gender = CC<unsigned>(character, 0x1C);
            return HighmountainControl(state, command, character, CreatureAppearance::Options(54, gender),
                gender ? femaleLabels : maleLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 62)
        {
            constexpr std::array<char const*, 5> labels =
                {"Skin Color", "Face", "Armor Style", "Armor Color", "Facial Features"};
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character,
                CreatureAppearance::Options(62, CC<unsigned>(character, 0x1C)), labels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 63)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::GnollControls,
                CreatureAppearance::GnollLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 64)
        {
            constexpr std::array<char const*, 5> labels =
                {"Fur Color", "Outfit Style", "Armor Color", "Cloth Color", "Facial Features"};
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::Options(64, 0),
                labels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 68)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return CC<unsigned>(character, 0x1C)
                ? HighmountainControl(state, command, character, CreatureAppearance::CentaurFemaleControls,
                    CreatureAppearance::CentaurFemaleLabels, requirements)
                : HighmountainControl(state, command, character, CreatureAppearance::CentaurMaleControls,
                    CreatureAppearance::CentaurMaleLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 65)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::SlimeControls,
                CreatureAppearance::SlimeLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 73)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::HozenControls,
                CreatureAppearance::HozenLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 74)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::VorkaiControls,
                CreatureAppearance::VorkaiLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 75)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::MantidControls,
                CreatureAppearance::MantidLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 76)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::EtherealControls,
                CreatureAppearance::EtherealLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 77)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::BansheeControls,
                CreatureAppearance::BansheeLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 78)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return HighmountainControl(state, command, character, CreatureAppearance::FacelessControls,
                CreatureAppearance::FacelessLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 79)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            return CC<unsigned>(character, 0x1C)
                ? HighmountainControl(state, command, character, CreatureAppearance::QirajiFemaleControls,
                    CreatureAppearance::QirajiLabels, requirements)
                : HighmountainControl(state, command, character, CreatureAppearance::QirajiControls,
                    CreatureAppearance::QirajiLabels, requirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 60 || RaceOf(CC<unsigned>(character, 0x18)) == 61)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            bool const female = CC<unsigned>(character, 0x1C) != 0;
            if (RaceOf(CC<unsigned>(character, 0x18)) == 60)
                return female ? HighmountainControl(state, command, character, CreatureAppearance::KyrianFemaleControls,
                        CreatureAppearance::KyrianFemaleLabels, requirements)
                    : HighmountainControl(state, command, character, CreatureAppearance::KyrianMaleControls,
                        CreatureAppearance::KyrianMaleLabels, requirements);
            return female ? HighmountainControl(state, command, character, CreatureAppearance::VenthyrFemaleControls,
                    CreatureAppearance::VenthyrFemaleLabels, requirements)
                : HighmountainControl(state, command, character, CreatureAppearance::VenthyrMaleControls,
                    CreatureAppearance::VenthyrMaleLabels, requirements);
        }
        if (CreatureAppearance::Uses(RaceOf(CC<unsigned>(character, 0x18))))
            return false;
        if (RaceOf(CC<unsigned>(character, 0x18)) == 20)
        {
            if (CC<unsigned>(character, 0x1C) == 0)
                return HighmountainControl(state, command, character, VulperaAppearance::MaleOptions,
                    VulperaAppearance::MaleLabels, VulperaAppearance::MaleRequirements);
            return HighmountainControl(state, command, character, VulperaAppearance::FemaleOptions,
                VulperaAppearance::FemaleLabels, VulperaAppearance::FemaleRequirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 50 || RaceOf(CC<unsigned>(character, 0x18)) == 51)
        {
            if (CC<unsigned>(character, 0x1C) == 0)
                return HighmountainControl(state, command, character, HaranirAppearance::MaleOptions,
                    HaranirAppearance::MaleLabels, HaranirAppearance::MaleRequirements);
            return HighmountainControl(state, command, character, HaranirAppearance::FemaleOptions,
                HaranirAppearance::FemaleLabels, HaranirAppearance::FemaleRequirements);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) != 46)
        {
            if (CC<unsigned>(character, 0x1C) == 0)
                return HighmountainControl(state, command, character, EarthenAppearance::MaleOptions,
                    EarthenAppearance::MaleLabels, EarthenAppearance::MaleRequirements);
            return HighmountainControl(state, command, character, EarthenAppearance::FemaleOptions,
                EarthenAppearance::FemaleLabels, EarthenAppearance::FemaleRequirements);
        }
        if (CC<unsigned>(character, 0x1C) == 0)
            return HighmountainControl(state, command, character, HighmountainAppearance::MaleOptions,
                HighmountainAppearance::MaleLabels, HighmountainAppearance::MaleRequirements);
        return HighmountainControl(state, command, character, HighmountainAppearance::FemaleOptions,
            HighmountainAppearance::FemaleLabels, HighmountainAppearance::FemaleRequirements);
    }

    struct HighmountainSelection
    {
        unsigned gender;
        unsigned kind; // 0: geometry group, 1: replaceable material
        unsigned target;
        unsigned value;
        unsigned choices[3]; // {option, value} packed as two uint16s; 0xffffffff means unconditional
        char path[128];
    };

    std::vector<HighmountainSelection> ReadSelections(wchar_t const* name)
    {
        return [name]
        {
            std::vector<HighmountainSelection> result;
            FILE* file = CatalogFile(name);
            if (!file)
                return result;
            unsigned header[3] = {};
            if (std::fread(header, sizeof(header), 1, file) == 1 && header[0] == 0x314D4845
                && header[1] == 1 && header[2] <= 16384)
                for (unsigned i = 0; i < header[2]; ++i)
                {
                    HighmountainSelection record{};
                    if (std::fread(&record, sizeof(record), 1, file) != 1)
                        break;
                    bool valid = record.gender < 2 && record.kind <= 5
                        && (record.kind == 5 ? record.target < 16 && record.value && record.path[0]
                            && std::memchr(record.path, 0, sizeof(record.path))
                            : record.kind == 4 ? record.target && record.value < 52
                            : record.kind == 3 ? record.target < 9 && record.value >= 16
                            && std::memchr(record.path, 0, sizeof(record.path))
                            : record.kind == 2 ? record.target < 5200 && record.value >= 10000
                            && record.value < 30000 : record.kind ? record.target < 16 && record.path[0]
                            && std::memchr(record.path, 0, sizeof(record.path)) : record.target < 52
                            && record.value >= record.target * 100 && record.value < record.target * 100 + 100);
                    for (unsigned selector : record.choices)
                        valid = valid && (selector == 0xffffffff || (selector & 0xffff) < 32
                            && (selector >> 16) < 256);
                    if (valid)
                        result.push_back(record);
                }
            std::fclose(file);
            return result;
        }();
    }

    std::vector<HighmountainSelection> const& HighmountainSelections()
    {
        static auto const records = ReadSelections(L"EsteriaHighmountain.bin");
        return records;
    }

    std::vector<HighmountainSelection> const& EarthenSelections()
    {
        static auto const records = ReadSelections(L"EsteriaEarthen.bin");
        return records;
    }

    std::vector<HighmountainSelection> const& HaranirSelections()
    {
        static auto const records = ReadSelections(L"EsteriaHaranir.bin");
        return records;
    }

    std::vector<HighmountainSelection> const& VulperaSelections()
    {
        static auto const records = ReadSelections(L"EsteriaVulpera.bin");
        return records;
    }

    std::vector<HighmountainSelection> const& RetailSelections(unsigned race)
    {
        static auto const naga = ReadSelections(L"EsteriaNaga.bin");
        static auto const tuskarr = ReadSelections(L"EsteriaTuskarr.bin");
        static auto const vrykul = ReadSelections(L"EsteriaVrykul.bin");
        static auto const human = ReadSelections(L"EsteriaThinHuman.bin");
        static auto const sethrak = ReadSelections(L"EsteriaSethrak.bin");
        static auto const gnoll = ReadSelections(L"EsteriaGnoll.bin");
        static auto const saberon = ReadSelections(L"EsteriaSaberon.bin");
        static auto const tarasek = ReadSelections(L"EsteriaTarasek.bin");
        static auto const drakonid = ReadSelections(L"EsteriaDrakonid.bin");
        static auto const kyrian = ReadSelections(L"EsteriaKyrian.bin");
        static auto const venthyr = ReadSelections(L"EsteriaVenthyr.bin");
        static auto const slime = ReadSelections(L"EsteriaSlime.bin");
        static auto const centaur = ReadSelections(L"EsteriaCentaur.bin");
        static auto const hozen = ReadSelections(L"EsteriaHozen.bin");
        static auto const vorkai = ReadSelections(L"EsteriaVorkai.bin");
        static auto const mantid = ReadSelections(L"EsteriaMantid.bin");
        static auto const ethereal = ReadSelections(L"EsteriaEthereal.bin");
        static auto const banshee = ReadSelections(L"EsteriaBanshee.bin");
        static auto const faceless = ReadSelections(L"EsteriaFaceless.bin");
        static auto const qiraji = ReadSelections(L"EsteriaQiraji.bin");
        switch (race)
        {
            case 54: return naga;
            case 55: return tuskarr;
            case 56:
            case 57: return vrykul;
            case 58:
            case 59: return human;
            case 62: return sethrak;
            case 63: return gnoll;
            case 64: return saberon;
            case 60: return kyrian;
            case 61: return venthyr;
            case 65: return slime;
            case 68: return centaur;
            case 73: return hozen;
            case 74: return vorkai;
            case 75: return mantid;
            case 76: return ethereal;
            case 77: return banshee;
            case 78: return faceless;
            case 79: return qiraji;
            case 66: return tarasek;
            case 69:
            case 70: return drakonid;
            case 20: return VulperaSelections();
            default: return HaranirSelections();
        }
    }

    wchar_t const* RetailTextureBank(unsigned race)
    {
        switch (race)
        {
            case 54: return L"EsteriaNagaTextures.bin";
            case 55: return L"EsteriaTuskarrTextures.bin";
            case 56:
            case 57: return L"EsteriaVrykulTextures.bin";
            case 58:
            case 59: return L"EsteriaThinHumanTextures.bin";
            case 62: return L"EsteriaSethrakTextures.bin";
            case 63: return L"EsteriaGnollTextures.bin";
            case 64: return L"EsteriaSaberonTextures.bin";
            case 60: return L"EsteriaKyrianTextures.bin";
            case 61: return L"EsteriaVenthyrTextures.bin";
            case 65: return L"EsteriaSlimeTextures.bin";
            case 68: return L"EsteriaCentaurTextures.bin";
            case 73: return L"EsteriaHozenTextures.bin";
            case 74: return L"EsteriaVorkaiTextures.bin";
            case 75: return L"EsteriaMantidTextures.bin";
            case 76: return L"EsteriaEtherealTextures.bin";
            case 77: return L"EsteriaBansheeTextures.bin";
            case 78: return L"EsteriaFacelessTextures.bin";
            case 79: return L"EsteriaQirajiTextures.bin";
            case 66: return L"EsteriaTarasekTextures.bin";
            case 69:
            case 70: return L"EsteriaDrakonidTextures.bin";
            case 20: return L"EsteriaVulperaTextures.bin";
            default: return L"EsteriaHaranirTextures.bin";
        }
    }

    char const* RetailCacheFamily(unsigned race)
    {
        switch (race)
        {
            case 54: return "Naga";
            case 55: return "Tuskarr";
            case 56:
            case 57: return "Vrykul";
            case 58:
            case 59: return "ThinHuman";
            case 62: return "Sethrak";
            case 63: return "Gnoll";
            case 64: return "Saberon";
            case 66: return "Tarasek";
            case 69:
            case 70: return "Drakonid";
            case 20: return "Vulpera";
            case 73: return "Hozen";
            case 74: return "Vorkai";
            case 75: return "Mantid";
            case 76: return "Ethereal";
            case 77: return "Banshee";
            case 78: return "Faceless";
            case 79: return "Qiraji";
            default: return "Haranir";
        }
    }

    unsigned TextureCoverage(void* character, wchar_t const* catalog)
    {
        auto read = [](wchar_t const* name)
        {
            std::unordered_map<unsigned, unsigned> result;
            FILE* file = CatalogFile(name);
            if (!file)
                return result;
            unsigned header[3]{};
            if (std::fread(header, sizeof(header), 1, file) == 1 && header[0] == 0x56434253
                && header[1] == 1 && header[2] <= 1048576) // CoA: every client item display, not only Esteria's
                for (unsigned i = 0; i < header[2]; ++i)
                {
                    unsigned row[2]{};
                    if (std::fread(row, sizeof(row), 1, file) != 1 || !row[0] || !row[1] || row[1] >= 128)
                    {
                        result.clear();
                        break;
                    }
                    result.emplace(row[0], row[1]);
                }
            std::fclose(file);
            return result;
        };
        static auto const saberon = read(L"EsteriaSaberonCoverage.bin");
        static auto const drakonid = read(L"EsteriaDrakonidCoverage.bin");
        auto const& masks = std::wcscmp(catalog, L"EsteriaDrakonidCoverage.bin") == 0 ? drakonid : saberon;
        unsigned result = 0;
        for (unsigned slot = 2; slot <= 9; ++slot)
        {
            if (slot == 4)
                continue; // Belts have a separate raised waist surface.
            auto found = masks.find(CC<unsigned>(character, 0x428 + slot * 4));
            if (found != masks.end())
                result |= found->second;
        }
        return result;
    }

    unsigned SaberonTextureCoverage(void* character)
    {
        return TextureCoverage(character, L"EsteriaSaberonCoverage.bin");
    }

    unsigned DrakonidTextureCoverage(void* character)
    {
        return TextureCoverage(character, L"EsteriaDrakonidCoverage.bin");
    }

#include "HaranirMaterials.inl"

    template<class Options>
    bool ExtendedGeometry(void* character, Options const& options,
        std::vector<HighmountainSelection> const& records)
    {
        unsigned gender = CC<unsigned>(character, 0x1C);
        auto const network = earthenNetworkAppearance.find(character);
        if (network != earthenNetworkAppearance.end())
            SetHighmountainFields(character, network->second);
        auto fields = HighmountainFields(character);
        if (!ValidateExtended(RaceOf(CC<unsigned>(character, 0x18)), gender, fields))
        {
            fields = {};
            SetHighmountainFields(character, fields);
        }
        void* instance = CC<void*>(character, 0x38);
        if (!instance)
            return true;
        bool nativeClothingCovered = false;
        if (RaceOf(CC<unsigned>(character, 0x18)) == 62 || RaceOf(CC<unsigned>(character, 0x18)) == 64)
            for (unsigned slot = 2; slot <= 9; ++slot)
                nativeClothingCovered = nativeClothingCovered || CC<unsigned>(character, 0x428 + slot * 4) != 0;
        unsigned materials = 0;
        std::uint64_t helmetGroups = 0;
        unsigned appearanceRace = RaceOf(CC<unsigned>(character, 0x18));
        if (appearanceRace == 20 || appearanceRace == 54 || appearanceRace == 66
            || appearanceRace == 69 || appearanceRace == 70)
            for (auto const& record : records)
                if (record.kind == 4 && record.gender == gender && record.value < 52
                    && record.target == CC<unsigned>(character, 0x428))
                    helmetGroups |= std::uint64_t{1} << record.value;
        std::uint64_t geometryGroups = 0;
        unsigned selectedGeosets[52] = {};
        N_82C7C0(instance, 10000, 29999, 0);
        for (auto const& record : records)
        {
            unsigned race = RaceOf(CC<unsigned>(character, 0x18));
            bool equipmentTexture = record.kind == 5
                && (race == 64 || race == 66 || race == 69 || race == 70)
                && ((record.target == 9 && record.value == CC<unsigned>(character, 0x438))
                    || (record.target == 10 && record.value == CC<unsigned>(character, 0x43C)));
            if (record.gender != gender || (record.kind >= 3 && !equipmentTexture))
                continue;
            bool selected = true;
            for (unsigned selector : record.choices)
            {
                if (selector == 0xffffffff)
                    continue;
                unsigned index = selector & 0xffff;
                if (index >= options.size())
                {
                    selected = false;
                    break;
                }
                auto const& option = options[index];
                selected = selected && (fields[option.field] / option.factor) % option.count == selector >> 16;
            }
            if (!selected)
                continue;
            if (record.kind == 2)
            {
                if (nativeClothingCovered && record.value >= 21000 && record.value < 22000)
                    continue;
                if ((race == 64 || race == 66 || race == 69 || race == 70)
                    && record.value >= 20001 && record.value <= 20009)
                {
                    unsigned coverage = race == 64 ? SaberonTextureCoverage(character)
                        : DrakonidTextureCoverage(character);
                    unsigned beltDisplay = CC<unsigned>(character, 0x438);
                    bool belt = std::any_of(records.begin(), records.end(), [beltDisplay](auto const& item)
                    {
                        return item.kind == 5 && item.target == 9 && item.value == beltDisplay;
                    });
                    unsigned pantsDisplay = CC<unsigned>(character, 0x43C);
                    bool separatePants = belt && std::any_of(records.begin(), records.end(),
                        [pantsDisplay](auto const& item)
                    {
                        return item.kind == 5 && item.target == 10 && item.value == pantsDisplay;
                    });
                    unsigned region = record.value == 20009 ? 5 : record.value - 20001;
                    bool covered = record.value == 20008 ? belt : (coverage & (1u << region)) != 0;
                    if (record.value == 20006 && separatePants)
                        covered = false;
                    if (record.value == 20009 && !separatePants)
                        covered = false;
                    if (!covered)
                        continue;
                }
                unsigned group = record.target / 100;
                unsigned source = group < 19 ? CC<unsigned>(character, 0x144 + group * 4)
                    : selectedGeosets[group];
                if (record.target == 0 || source == record.target)
                    N_82C7C0(
                        instance, record.value, record.value, 1);
                continue;
            }
            if (!record.kind)
            {
                if (geometryGroups & (std::uint64_t{1} << record.target))
                    continue;
                geometryGroups |= std::uint64_t{1} << record.target;
                unsigned selectedValue = record.value;
                if (helmetGroups & (std::uint64_t{1} << record.target))
                    selectedValue = record.target * 100;
                unsigned race = RaceOf(CC<unsigned>(character, 0x18));
                if (race == 63)
                {
                    // Native Gnoll outfits yield only to equipment covering the same body region.
                    auto equipped = [&](unsigned slot)
                    {
                        return CC<unsigned>(character, 0x428 + slot * 4) != 0;
                    };
                    unsigned group = record.target;
                    bool covered = (group == 35 && (equipped(2) || equipped(3) || equipped(9)))
                        || (group == 36 && (equipped(7) || equipped(8)))
                        || ((group == 37 || group == 42) && equipped(4))
                        || ((group == 38 || group == 43) && (equipped(5) || equipped(3)   // CoA: chest too (robes:
                            || CC<unsigned>(character, 0x178) == 1302))                 // 1302 never set here)
                        || (group == 40 && (equipped(5) || equipped(6))) || (group == 41 && equipped(1));
                    if (covered)
                        // These defaults are authored body surfaces, not removable outfit decorations.
                        selectedValue = group == 37 ? 3701 : group == 38 ? 3801 : group * 100;
                }
                if (record.target == 20 && (race == 48 || race == 49 || race == 50 || race == 51
                    || race == 56 || race == 57))
                    // Native equipment slot7 (Feet) maps to component visual slot6 at 0x428 + 6*4.
                    selectedValue = CC<unsigned>(character, 0x440) ? 2002
                        : (race == 50 || race == 51 ? record.value : 2001);
                selectedGeosets[record.target] = selectedValue;
                if (record.target < 19)
                    CC<unsigned>(character, 0x144 + record.target * 4) = selectedValue;
                else
                {
                    N_82C7C0(
                        instance, record.target * 100, record.target * 100 + 99, 0);
                    N_82C7C0(
                        instance, selectedValue, selectedValue, 1);
                }
            }
            else if (!(materials & (1u << record.target)))
            {
                void* texture = N_4E8D30(
                    record.path, nullptr);
                if (texture)
                {
                    N_825260(instance, record.target, texture);
                    N_47BF30(texture);
                }
                materials |= 1u << record.target;
            }
        }
        // CoA: Gnoll skirt (43xx) and loincloth (38xx) are drawn by default when no record picks them; under a chest
        // piece or trousers they took the robe paint as a stiff cylinder. Hide both groups outright then.
        if (RaceOf(CC<unsigned>(character, 0x18)) == 63
            && (CC<unsigned>(character, 0x428 + 3 * 4) || CC<unsigned>(character, 0x428 + 5 * 4)))
        {
            N_82C7C0(instance, 4300, 4399, 0);
            N_82C7C0(instance, 3800, 3899, 0);
        }
        // Kyrian / Venthyr: equipped armor is baked onto the native body (KvOutfits.inl). Until it is bound keep the
        // player robe hidden and give the raw garment atlas (texture type 15) the empty garment.
        if (RaceOf(CC<unsigned>(character, 0x18)) == 60 || RaceOf(CC<unsigned>(character, 0x18)) == 61)
        {
            N_82C7C0(instance, 1302, 1302, 0);
            void* empty = N_4E8D30(
                "custom\\equipment\\native\\empty-garment.blp", nullptr);
            if (empty)
            {
                N_825260(instance, 15, empty);
                N_47BF30(empty);
            }
            KvBindOutfit(character);
        }
        if (RaceOf(CC<unsigned>(character, 0x18)) == 65)
            KvSlimeBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 68)
            KvCentaurBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 73)
            KvHozenBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 74)
            KvVorkaiBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 75)
            KvMantidBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 76)
            KvEtherealBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 77)
            KvBansheeBind(character);
        if (RaceOf(CC<unsigned>(character, 0x18)) == 78)
            KvFacelessBind(character);
        // Some source-hidden groups have no customization selector. Apply their hide after all selections.
        for (unsigned group = 0; group < 52; ++group)
            if (helmetGroups & (std::uint64_t{1} << group))
            {
                if (group < 19)
                    CC<unsigned>(character, 0x144 + group * 4) = group * 100;
                N_82C7C0(
                    instance, group * 100, group * 100 + 99, 0);
            }
        return true;
    }

    void BindDrakkinFemale(void* character)
    {
        // koboldfemale body meshes use replaceable type 8. Stock character sections only fill type 1,
        // so the body stayed black while the head kept whatever type 1 resolved. extra-N is the painted
        // albedo; body-N is a dark mask. Apply the selected painted skin to both slots.
        void* instance = CC<void*>(character, 0x38);
        if (!instance)
            return;
        unsigned skin = CC<unsigned>(character, 0x28);
        if (skin > 15)
            skin = CC<unsigned>(character, 0x24);
        if (skin > 15)
            skin = 0;
        char path[96];
        std::snprintf(path, sizeof(path), "custom\\kobold\\extra-%u.blp", skin);
        void* texture = N_4E8D30(
            path, nullptr);
        if (!texture)
            return;
        for (unsigned slot : {8u})  // type 1 stays the engine composite of body-N (full-body mesh 9901 + armour shells)
            N_825260(instance, slot, texture);
        N_47BF30(texture);
    }

    bool HighmountainGeometry(void* character)
    {
        unsigned race = RaceOf(CC<unsigned>(character, 0x18));
        if (!ExtendedRace(race))
            return false;
        // The authored female player model uses ordinary DBC materials and equipment geosets.
        // Paint her body before returning so type 8 is no longer left unbound.
        if (race == 66 && CC<unsigned>(character, 0x1C) == 1)
            BindDrakkinFemale(character);
        if (CreatureAppearance::UsesPlayerVrykul(race, CC<unsigned>(character, 0x1C)))
            return false;
        if (race != 46 && CC<void*>(character, 0x38))
            CaptureEarthenOwner(character);
        if (CreatureAppearance::Uses(race))
        {
            SetHaranirMaterials(character);
            if (race == 63)
                return ExtendedGeometry(character, CreatureAppearance::GnollControls, RetailSelections(race));
            if (race == 65)
                return ExtendedGeometry(character, CreatureAppearance::SlimeControls, RetailSelections(race));
            if (race == 73)
                return ExtendedGeometry(character, CreatureAppearance::HozenControls, RetailSelections(race));
            if (race == 74)
                return ExtendedGeometry(character, CreatureAppearance::VorkaiControls, RetailSelections(race));
            if (race == 75)
                return ExtendedGeometry(character, CreatureAppearance::MantidControls, RetailSelections(race));
            if (race == 76)
                return ExtendedGeometry(character, CreatureAppearance::EtherealControls, RetailSelections(race));
            if (race == 77)
                return ExtendedGeometry(character, CreatureAppearance::BansheeControls, RetailSelections(race));
            if (race == 78)
                return ExtendedGeometry(character, CreatureAppearance::FacelessControls, RetailSelections(race));
            if (race == 79)
                return CC<unsigned>(character, 0x1C)
                    ? ExtendedGeometry(character, CreatureAppearance::QirajiFemaleControls, RetailSelections(race))
                    : ExtendedGeometry(character, CreatureAppearance::QirajiControls, RetailSelections(race));
            if (race == 68)
                return CC<unsigned>(character, 0x1C) ? ExtendedGeometry(character, CreatureAppearance::CentaurFemaleControls, RetailSelections(race))
                    : ExtendedGeometry(character, CreatureAppearance::CentaurMaleControls, RetailSelections(race));
            if (race == 60 || race == 61)
            {
                bool const female = CC<unsigned>(character, 0x1C) != 0;
                if (race == 60)
                    return female ? ExtendedGeometry(character, CreatureAppearance::KyrianFemaleControls, RetailSelections(race))
                        : ExtendedGeometry(character, CreatureAppearance::KyrianMaleControls, RetailSelections(race));
                return female ? ExtendedGeometry(character, CreatureAppearance::VenthyrFemaleControls, RetailSelections(race))
                    : ExtendedGeometry(character, CreatureAppearance::VenthyrMaleControls, RetailSelections(race));
            }
            return ExtendedGeometry(character, CreatureAppearance::Options(race, CC<unsigned>(character, 0x1C)),
                RetailSelections(race));
        }
        if (race == 20)
        {
            SetHaranirMaterials(character);
            if (CC<unsigned>(character, 0x1C) == 0)
                return ExtendedGeometry(character, VulperaAppearance::MaleOptions, VulperaSelections());
            return ExtendedGeometry(character, VulperaAppearance::FemaleOptions, VulperaSelections());
        }
        if (race == 50 || race == 51)
        {
            SetHaranirMaterials(character);
            if (CC<unsigned>(character, 0x1C) == 0)
                return ExtendedGeometry(character, HaranirAppearance::MaleOptions, HaranirSelections());
            return ExtendedGeometry(character, HaranirAppearance::FemaleOptions, HaranirSelections());
        }
        if (race == 46)
        {
            if (CC<unsigned>(character, 0x1C) == 0)
                return ExtendedGeometry(character, HighmountainAppearance::MaleOptions, HighmountainSelections());
            return ExtendedGeometry(character, HighmountainAppearance::FemaleOptions, HighmountainSelections());
        }
        if (CC<unsigned>(character, 0x1C) == 0)
            return ExtendedGeometry(character, EarthenAppearance::MaleOptions, EarthenSelections());
        return ExtendedGeometry(character, EarthenAppearance::FemaleOptions, EarthenSelections());
    }
}

void EsteriaCreateExtra(void* packet)
{
    void* character = CreationCharacter();
    if (!packet || !character || !ExtendedRace(RaceOf(CC<unsigned>(character, 0x18)))
        || RaceOf(CC<unsigned>(character, 0x18)) == 20 || CreatureAppearance::Uses(RaceOf(CC<unsigned>(character, 0x18))))
        return;
    unsigned size = static_cast<CDataStore*>(packet)->m_size;
    unsigned base = static_cast<CDataStore*>(packet)->m_base;
    if (!size || size < base || size - base > static_cast<CDataStore*>(packet)->m_alloc)
        return;
    auto* bytes = static_cast<CDataStore*>(packet)->m_data;
    auto fields = HighmountainFields(character);
    unsigned race = RaceOf(CC<unsigned>(character, 0x18));
    bytes[size - base - 1] = race == 50 || race == 51 ? 0 : fields[5];
    if (race == 50 || race == 51)
    {
        auto const append = N_47AFE0;
        for (unsigned i = 5; i < fields.size(); ++i)
            append(packet, fields[i]);
        for (unsigned i = 0; i < 4; ++i)
            append(packet, (0x31435248 >> (i * 8)) & 255);
    }
}

void EsteriaEnumExtra(void* packet)
{
    highmountainRoster.clear();
    cosmeticWingRoster.clear();
    BodyShape::Roster().clear();
    RetailRaceClearLooks();
    if (!packet)
        return;
    unsigned length = static_cast<CDataStore*>(packet)->m_size;
    unsigned base = static_cast<CDataStore*>(packet)->m_base;
    if (length < base || length - base < 8 || length - base > static_cast<CDataStore*>(packet)->m_alloc)
        return;
    auto* bytes = static_cast<CDataStore*>(packet)->m_data;
    unsigned size = length - base;
    if (!bytes)
        return;
    for (unsigned tail = 0; tail < 6 && size >= 8; ++tail)
    {
        unsigned magic = Field<unsigned>(bytes, size - 4);
        unsigned stride = magic == 0x31455848 ? 9 : magic == 0x32455848 ? 16 : magic == 0x31475743 ? 12
            : magic == CharacterBodyShape::EnumMagic ? CharacterBodyShape::EnumStride
            : magic == CharacterRetailLook::EnumMagic || magic == CharacterRetailLook::ParkedMagic ? CharacterRetailLook::EnumStride : 0;
        if (!stride)
            break;
        unsigned count = Field<unsigned>(bytes, size - 8);
        if (count > 100 || count > (size - 8) / stride)
            break;
        unsigned start = size - 8 - count * stride;
        for (unsigned i = 0; i < count; ++i)
        {
            auto guid = Field<std::uint64_t>(bytes, start + i * stride);
            if (stride == 12)
                cosmeticWingRoster[guid] = Field<unsigned>(bytes, start + i * stride + 8);
            else if (stride == CharacterRetailLook::EnumStride)
            {
                CharacterRetailLook::Look look;
                if (CharacterRetailLook::ReadPayload(bytes + start + i * stride + 8, CharacterRetailLook::PayloadSize, look))
                {
                    if (magic == CharacterRetailLook::ParkedMagic)
                        RetailRaceSetParkedLook(guid, look);
                    else
                        RetailRaceSetLook(guid, look);
                }
            }
            else if (stride == CharacterBodyShape::EnumStride)
            {
                CharacterBodyShape::Values wire{};
                if (CharacterBodyShape::ReadPayload(bytes + start + i * stride + 8, CharacterBodyShape::PayloadSize, wire))
                    BodyShape::Roster()[guid] = BodyShape::FromWire(wire);
            }
            else
                highmountainRoster[guid] = stride == 9
                    ? bytes[start + i * stride + 8] : Field<std::uint64_t>(bytes, start + i * stride + 8);
        }
        size = start;
        static_cast<CDataStore*>(packet)->m_size = base + size;
    }
}

void EsteriaSelectExtra(void* character)
{
    if (!character)
        return;
    unsigned count = SelCount();
    if (count > 100)
        return;
    for (unsigned i = 0; i < count; ++i)
    {
        auto* row = SelRow(i);
        if (Field<void*>(row, 0x188) != character)
            continue;
        RetailRaceApplyLookTo(static_cast<CCharacterComponent*>(character), Field<std::uint64_t>(row, 0), false); // CoA: the retail look saved for this character
        auto wing = cosmeticWingRoster.find(Field<std::uint64_t>(row, 0));
        cosmeticWingSelections[character] = wing == cosmeticWingRoster.end() ? 0 : wing->second;
        UpdateCosmeticWing(character);
        unsigned realRace = row[0x178];
        unsigned race = RaceOf(realRace);
        if (!ExtendedRace(race))
            return;
        auto it = highmountainRoster.find(Field<std::uint64_t>(row, 0));
        highmountainExtra[character] = race == 20 || CreatureAppearance::Uses(race) ? 0
            : it != highmountainRoster.end() ? it->second : 0;
        if (it == highmountainRoster.end() && (race == 48 || race == 49 || race == 50 || race == 51))
        {
            // CoA: no roster extension for this character (the server sends none): every Earthen / Haranir in the list looked the
            // same, so give each a stable pseudo-random look from its GUID (splitmix64)
            std::uint64_t z = Field<std::uint64_t>(row, 0) + 0x9E3779B97F4A7C15ull;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            highmountainExtra[character] = (z ^ (z >> 31)) & 0x0000FFFFFFFFFFFFull;
        }
        if (race == 20 || race == 48 || race == 49 || race == 50 || race == 51 || CreatureAppearance::Uses(race))
        {
            // The preview's first base-section validation precedes the stock setters that establish
            // resolver context. Seed the authenticated roster identity/bytes before that validation.
            CC<unsigned>(character, 0x18) = realRace;
            CC<unsigned>(character, 0x1C) = row[0x17A];
            CaptureEarthenAppearance(character, row + 0x17B, highmountainExtra[character]);
            auto const appearance = earthenNetworkAppearance.find(character);
            if (appearance != earthenNetworkAppearance.end())
                SetHighmountainFields(character, appearance->second);
            highmountainContext = character;
        }
        return;
    }
}

void EsteriaUnitExtra(void* unit)
{
    if (!unit || (UnitTypeId(unit) != 3 && UnitTypeId(unit) != 4))
        return;
    void* character = UnitComponent(unit);
    void* fields = UnitFields(unit); // Native UnitFields starts after the six ObjectFields.
    if (!character || !fields || !ExtendedRace(RaceOf(CC<unsigned>(character, 0x18))))
        return;
    unsigned race = RaceOf(CC<unsigned>(character, 0x18));
    highmountainExtra[character] = race == 20 || CreatureAppearance::Uses(race) ? 0 : race == 50 || race == 51
        ? UnitAppearanceExtra(unit) : Field<unsigned>(fields, 0x8D * 4) & 255;
    // Stock player initialization clamps packed choices against sparse ordinary DBC counts.
    // Preserve the authenticated player bytes before the native compositor and accessory selectors run.
    if (UnitTypeId(unit) == 4)
    {
        auto* playerData = PlayerData(unit);
        if (playerData)
            CaptureEarthenAppearance(character, playerData + 0x14, highmountainExtra[character]);
    }
    HighmountainGeometry(character);
}

void EsteriaRegisterExtra()
{
    // UNIT_FIELD_PADDING has no stock old-value cache slot. Registering it makes the client copy
    // to the lookup's out-of-range fallback, corrupting adjacent objects. Read it after unit updates.
}

void EsteriaForgetCharacter(void* character)
{
    ForgetCosmeticWing(character);
    cosmeticWingSelections.erase(character);
    haranirImages.erase(character);
    kvOutfits.erase(character);
    earthenNetworkAppearance.erase(character);
    highmountainExtra.erase(character);
    if (highmountainContext == character)
        highmountainContext = nullptr;
}

void EsteriaAppearanceContext(void* character)
{
    highmountainContext = character;
}

void EsteriaSectionArguments(unsigned* arguments)
{
    if (!arguments || !ExtendedRace(RaceOf(arguments[1])) || arguments[2] > 1 || !highmountainContext
        || CC<unsigned>(highmountainContext, 0x18) != arguments[1]
        || CC<unsigned>(highmountainContext, 0x1C) != arguments[2])
        return;
    auto fields = HighmountainFields(highmountainContext);
    unsigned gender = arguments[2];
    if (CreatureAppearance::Uses(RaceOf(arguments[1])))
        return;
    if (RaceOf(arguments[1]) == 20 || RaceOf(arguments[1]) == 50 || RaceOf(arguments[1]) == 51)
    {
        arguments[4] = 0;
        arguments[5] = 0;
        return;
    }
    if (RaceOf(arguments[1]) != 46)
    {
        arguments[4] = arguments[3] == 1 ? fields[1] % 10 : 0;
        arguments[5] = arguments[3] == 3 ? fields[3] % 14 : arguments[3] == 2 ? 0 : fields[0] % 14;
        return;
    }
    // Resolve actual logical choices before the native compositor indexes CharSections.
    unsigned skin = fields[0] % 9;
    unsigned face = fields[1] % (gender ? 4 : 5);
    unsigned paint = gender ? (fields[1] / 4) % 4 : fields[3] / 64;
    unsigned paintColor = gender ? fields[2] / 81 : fields[5] % 3;
    if (arguments[3] == 0 || arguments[3] == 1 || arguments[3] == 4)
    {
        arguments[4] = arguments[3] == 1 ? face : 0;
        arguments[5] = skin + 9 * (paint + 4 * paintColor);
    }
    else
    {
        arguments[4] = 0;
        arguments[5] = 0;
    }
}

bool EsteriaDirectSection(
    void* character, unsigned operation, unsigned const* arguments)
{
    if (!character || !ExtendedRace(RaceOf(CC<unsigned>(character, 0x18))) || !arguments || operation > 2)
        return false;
    highmountainContext = character;
    unsigned kind = operation == 0 ? 0 : operation == 1 ? 3 : arguments[0];
    unsigned slot = operation == 0 ? 1 : operation == 1 ? 0 : arguments[1];
    if (kind > 4 || slot > 2)
        return true;
    unsigned style = operation == 2 ? arguments[2] : 0;
    unsigned color = operation == 2 ? arguments[3] : CC<unsigned>(character, 0x28);
    if (operation == 1 && CreatureAppearance::UsesPlayerVrykul(
        RaceOf(CC<unsigned>(character, 0x18)), CC<unsigned>(character, 0x1C)))
    {
        style = CC<unsigned>(character, 0x34);
        color = CC<unsigned>(character, 0x24);
    }
    char const* path = HaranirLayerPath(character, kind, slot);
    if (!path)
    {
        auto* table = ChrVarTable();
        auto* row = N_4F3BA0(table, CC<unsigned>(character, 0x18), CC<unsigned>(character, 0x1C),
                kind, style, color, nullptr);
        if (!row)
            return true;
        path = row->m_textureName[slot];
    }
    if (!path)
        return true;
    if (operation < 2)
    {
        unsigned race = RaceOf(CC<unsigned>(character, 0x18));
        if (race == 20 || (CreatureAppearance::Uses(race)
            && !CreatureAppearance::UsesPlayerVrykul(race, CC<unsigned>(character, 0x1C))))
        {
            SetHaranirMaterials(character);
            return true;
        }
        void* instance = CC<void*>(character, 0x38);
        if (instance && path[0])
        {
            void* texture = N_4E8D30(
                path, nullptr);
            if (texture)
            {
                N_825260(
                    instance, operation == 0 ? 8 : 6, texture);
                N_47BF30(texture);
            }
        }
        return true;
    }
    // The stock layer setter validates, then bypasses the resolver and reindexes encoded bytes.
    // Keep its cache/refcount/dirty-bit lifecycle, with the resolved row as the only material source.
    void*& layer = CC<void*>(character, 0x194 + (kind * 3 + slot) * 4);
    if (layer)
        N_4F31A0(layer);
    layer = nullptr;
    unsigned race = RaceOf(CC<unsigned>(character, 0x18));
    bool nativeMaterials = race == 20 || race == 50 || race == 51 || (CreatureAppearance::Uses(race)
        && !CreatureAppearance::UsesPlayerVrykul(race, CC<unsigned>(character, 0x1C)));
    layer = path[0] ? (nativeMaterials ? HaranirLoadLayer(path)
        : N_4F3930(path)) : nullptr;
    if (arguments[4] < 32)
        CC<unsigned>(character, 0x0C) |= 1u << arguments[4];
    void* pending = CC<void*>(character, 0x52C);
    if (pending)
    {
        Field<unsigned>(pending, 0) &= ~1u;
        CC<void*>(character, 0x52C) = nullptr;
    }
    CC<unsigned>(character, 8) &= ~8u;
    return true;
}

unsigned EsteriaSectionCount(unsigned const* arguments)
{
    // Race46 stores encoded bytes. Its material getters normalize every access into the sparse source table.
    if (!arguments || !ExtendedRace(RaceOf(arguments[1])) || arguments[2] > 1 || arguments[3] > 4)
        return 0xffffffff;
    if (CreatureAppearance::Uses(RaceOf(arguments[1])))
    {
        if (!CreatureAppearance::GenderAllowed(RaceOf(arguments[1]), arguments[2]))
            return 0;
        if (RaceOf(arguments[1]) == 69 || RaceOf(arguments[1]) == 70)
            return arguments[3] == 2 || arguments[3] == 3 ? 76 : 17;
        auto const& options = CreatureAppearance::Options(RaceOf(arguments[1]), arguments[2]);
        return arguments[3] == 2 || arguments[3] == 3 ? options[3].count : options[0].count;
    }
    if (RaceOf(arguments[1]) == 20)
    {
        auto const& capacities = arguments[2] ? VulperaAppearance::FemaleCapacities
            : VulperaAppearance::MaleCapacities;
        return arguments[3] == 2 || arguments[3] == 3 ? capacities[3] : capacities[0];
    }
    if (RaceOf(arguments[1]) == 50 || RaceOf(arguments[1]) == 51)
    {
        auto const& capacities = arguments[2] ? HaranirAppearance::FemaleCapacities
            : HaranirAppearance::MaleCapacities;
        return arguments[3] == 2 || arguments[3] == 3 ? capacities[3] : capacities[0];
    }
    if (RaceOf(arguments[1]) != 46)
    {
        auto const& capacities = arguments[2] ? EarthenAppearance::FemaleCapacities
            : EarthenAppearance::MaleCapacities;
        return arguments[3] == 2 || arguments[3] == 3 ? capacities[3] : capacities[0];
    }
    auto const& capacities = arguments[2] ? HighmountainAppearance::FemaleCapacities
        : HighmountainAppearance::MaleCapacities;
    return arguments[3] == 2 || arguments[3] == 3 ? capacities[3] : capacities[0];
}

#include "CursorSwap.inl"



#include "Engine/Source/Model2/EsteriaHooks.h"

namespace CoA::Esteria {
    int Cycle(void* lua) { return EsteriaCycle(lua); }
    unsigned RaceOf(unsigned race) { return ::RaceOf(race); }
    unsigned SkillRace(unsigned race) { return ::EsteriaSkillRace(race); }
    void Skin(void* shared) { EsteriaSkin(shared); }
    unsigned WideSource(const void* section, const void* skin) { return EsteriaWideSource(section, skin); }
    unsigned DrawStart(const void* section, void* instance) { return EsteriaDrawStart(section, instance); }
    void Geometry(void* character) { EsteriaGeometry(character); }
    void CreateExtra(void* packet) { EsteriaCreateExtra(packet); }
    void EnumExtra(void* packet) { EsteriaEnumExtra(packet); }
    void SelectExtra(void* character) { EsteriaSelectExtra(character); }
    void UnitExtra(void* unit) { EsteriaUnitExtra(unit); }
    void ForgetCharacter(void* character) { EsteriaForgetCharacter(character); }
    void AppearanceContext(void* character) { EsteriaAppearanceContext(character); }
    void SectionArguments(unsigned* arguments) { EsteriaSectionArguments(arguments); }
    bool DirectSection(void* character, unsigned operation, unsigned const* arguments) { return EsteriaDirectSection(character, operation, arguments); }
    unsigned SectionCount(unsigned const* arguments) { return EsteriaSectionCount(arguments); }

    void Init() {
        InstallCrashTrace();
        BodySliders::Init();
        g_esteriaModelHooks.wideSource = [](const M2SkinSection* section, const M2SkinProfile* skin) -> uint32_t { return EsteriaWideSource(section, skin); };
        g_esteriaModelHooks.skin = [](CM2Shared* shared) { EsteriaSkin(shared); };
        g_esteriaModelHooks.drawStart = [](const M2SkinSection* section, CM2Model* instance) -> uint32_t { return EsteriaDrawStart(section, instance); };
    }
}
