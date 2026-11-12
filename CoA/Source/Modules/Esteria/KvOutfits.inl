// Kyrian (60) / Venthyr (61): armor on the native bodies. Ported from EsteriaSelectorOutfits.inl of the 64-bit client.
// The engine composites the equipped items into per-region layers (CCharacterComponent::m_section); we paint those
// layers on a 512x512 player atlas, remap it onto the native 1024x512 body skin with the race's fitting map, and
// bind the result as texture type 11. The separate garment meshes (robe, boots, gloves, leg plates) use the plain
// atlas as texture type 15. Included in NativeAppearance.cpp's private namespace after HaranirMaterials.inl.
void KvLog(char const* format, ...)
{
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, executable, MAX_PATH))
        return;
    char* slash = std::strrchr(executable, '\\');
    if (!slash)
        return;
    std::strcpy(slash + 1, "KvOutfit.log");
    FILE* file = nullptr;
    if (fopen_s(&file, executable, "ab"))
        return;
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(file, format, arguments);
    va_end(arguments);
    std::fputc(10, file);
    std::fclose(file);
}

struct KvFittingMap
{
    unsigned width = 0;
    unsigned height = 0;
    std::vector<unsigned> pixels;
    std::vector<unsigned char> coverage;
};

struct KvOutfit
{
    std::array<unsigned, 12> gear{};
    std::array<std::uint8_t, 13> fields{};
    std::shared_ptr<void> texture;
    std::shared_ptr<void> garment;
    bool robe = false;
    bool shoes = false;
    bool greaves = false;
    bool gloves = false;
    bool legMeshes = false;
    unsigned gloveStyle = 0;
    unsigned bootStyle = 0;
    unsigned leg[4]{};
};

std::unordered_map<void*, KvOutfit> kvOutfits;

struct KvBodySkin
{
    unsigned width = 0;
    unsigned height = 0;
    std::vector<unsigned char> rgba;
};

bool KvReadAll(wchar_t const* name, std::vector<unsigned char>& bytes)
{
    FILE* file = CatalogFile(name);
    if (!file)
        return false;
    std::fseek(file, 0, SEEK_END);
    long length = std::ftell(file);
    std::rewind(file);
    bytes.resize(length > 0 ? length : 0);
    bool ok = length > 0 && std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    return ok;
}

std::array<int, 3> KvItemGeoset(unsigned display)
{
    static std::vector<unsigned char> bank;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        if (!KvReadAll(L"EsteriaKvItems.bin", bank))
            bank.clear();
    }
    std::array<int, 3> none{};
    if (bank.size() < 8 || Field<unsigned>(bank.data(), 0) != 0x3149564B)
        return none;
    unsigned count = Field<unsigned>(bank.data(), 4);
    if (bank.size() < 8 + static_cast<std::size_t>(count) * 16)
        return none;
    unsigned low = 0;
    unsigned high = count;
    while (low < high)
    {
        unsigned middle = (low + high) / 2;
        auto* row = bank.data() + 8 + middle * 16;
        unsigned id = Field<unsigned>(const_cast<unsigned char*>(row), 0);
        if (id == display)
            return {Field<int>(const_cast<unsigned char*>(row), 4), Field<int>(const_cast<unsigned char*>(row), 8),
                Field<int>(const_cast<unsigned char*>(row), 12)};
        if (id < display)
            low = middle + 1;
        else
            high = middle;
    }
    return none;
}

// The banks are indexed once and every skin is read and decoded on demand: they hold ~260 MB of compressed skins, too much to
// keep resident in the 32-bit client. EsteriaKvSkins carries the authored Esteria races, EsteriaPackSkins the AllRaces pack.
KvBodySkin const* KvSkin(char const* path)
{
    struct Entry { std::string name; unsigned width, height, size; long offset; };
    struct Bank { FILE* file = nullptr; std::vector<Entry> index; };
    static Bank banks[2];
    static bool loaded = false;
    static std::unordered_map<std::string, KvBodySkin> cache;
    if (!loaded)
    {
        loaded = true;
        wchar_t const* const names[2] = {L"EsteriaKvSkins.bin", L"EsteriaPackSkins.bin"};
        for (unsigned b = 0; b < 2; ++b)
        {
            Bank& bank = banks[b];
            bank.file = CatalogFile(names[b]);
            unsigned head[2]{};
            if (!bank.file || std::fread(head, sizeof(head), 1, bank.file) != 1 || head[0] != 0x3153564B)
                continue;
            for (unsigned i = 0; i < head[1]; ++i)
            {
                char header[144]{};
                if (std::fread(header, sizeof(header), 1, bank.file) != 1)
                    break;
                Entry entry{std::string(header, strnlen(header, 128)), Field<unsigned>(header, 128), Field<unsigned>(header, 132),
                    Field<unsigned>(header, 136), std::ftell(bank.file)};
                bank.index.push_back(entry);
                if (std::fseek(bank.file, static_cast<long>(entry.size), SEEK_CUR))
                    break;
            }
        }
    }
    std::string key = path;
    for (auto& c : key)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto found = cache.find(key);
    if (found != cache.end())
        return found->second.rgba.empty() ? nullptr : &found->second;
    if (cache.size() >= 12)
        cache.clear();      // decoded skins are 1-2 MB each: do not keep every colour the player ever looked at
    auto& entry = cache[key];
    auto decompress = reinterpret_cast<LONG(WINAPI*)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, PULONG)>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlDecompressBuffer"));
    for (Bank const& bank : banks)
    {
        for (auto const& item : bank.index)
        {
            if (item.name != key)
                continue;
            if (!decompress || !bank.file || !item.width || !item.height || item.width > 2048 || item.height > 2048)
                return nullptr;
            std::vector<unsigned char> packed(item.size);
            if (std::fseek(bank.file, item.offset, SEEK_SET) || std::fread(packed.data(), 1, packed.size(), bank.file) != packed.size())
                return nullptr;
            entry.rgba.resize(static_cast<std::size_t>(item.width) * item.height * 4);
            ULONG final = 0;
            if (decompress(2, entry.rgba.data(), static_cast<ULONG>(entry.rgba.size()), packed.data(), item.size, &final) != 0
                || final != entry.rgba.size())
            {
                entry.rgba.clear();
                return nullptr;
            }
            entry.width = item.width;
            entry.height = item.height;
            return &entry;
        }
    }
    return nullptr;
}

KvFittingMap const* KvMap(unsigned race, unsigned gender)
{
    static std::unordered_map<unsigned, KvFittingMap> maps;
    static std::unordered_map<unsigned, bool> tried;
    unsigned index = race * 2 + gender;
    auto& map = maps[index];
    if (!tried[index])
    {
        tried[index] = true;
        wchar_t path[128]{};
        swprintf_s(path, L"Data\\Retail\\Assets\\EquipmentFitting\\%u-%u.bin", race, gender);
        FILE* file = CatalogFile(path);
        if (!file)
            return nullptr;
        unsigned header[6]{};
        bool valid = std::fread(header, sizeof(header), 1, file) == 1 && header[0] == 0x3146554e && header[1] == 1
            && header[2] && header[2] <= 1024 && header[3] && header[3] <= 1024 && header[4] == 512 && header[5] == 512;
        if (valid)
        {
            map.width = header[2];
            map.height = header[3];
            map.pixels.resize(static_cast<std::size_t>(map.width) * map.height);
            valid = std::fread(map.pixels.data(), sizeof(unsigned), map.pixels.size(), file) == map.pixels.size()
                && std::all_of(map.pixels.begin(), map.pixels.end(), [](unsigned p)
                {
                    return p == 0xffffffffu || ((p & 65535u) < 512 && (p >> 16) < 512);
                });
            if (valid && ((race == 60 && gender == 1) || race == 61))
            {
                map.coverage.resize(map.pixels.size());
                if (std::fread(map.coverage.data(), 1, map.coverage.size(), file) != map.coverage.size()
                    || !std::all_of(map.coverage.begin(), map.coverage.end(), [](unsigned char v) { return v <= 4; }))
                    map.coverage.clear();
            }
        }
        std::fclose(file);
        if (!valid)
            map = KvFittingMap{};
    }
    return map.pixels.empty() ? nullptr : &map;
}

void KvOver(unsigned char* dst, unsigned char const* src)
{
    unsigned a = src[3];
    unsigned prior = dst[3];
    unsigned total = a * 255 + prior * (255 - a);
    for (unsigned channel = 0; channel < 3; ++channel)
        dst[channel] = static_cast<unsigned char>(
            total ? (src[channel] * a * 255 + dst[channel] * prior * (255 - a) + total / 2) / total : 0);
    dst[3] = static_cast<unsigned char>((total + 127) / 255);
}

bool KvSealBootFoot(std::vector<unsigned char>& pixels)
{
    std::array<int, 64> rows{};
    rows.fill(-1);
    for (int y = 0; y < 64; ++y)
    {
        std::array<int, 256> valid{};
        unsigned count = 0;
        auto* row = pixels.data() + 4 * ((448 + y) * 512 + 256);
        for (int x = 0; x < 256; ++x)
            if (row[4 * x + 3] >= 128)
                valid[count++] = x;
        if (!count)
            continue;
        rows[y] = y;
        auto original = std::vector<unsigned char>(row, row + 1024);
        for (int x = 0; x < 256; ++x)
        {
            if (row[4 * x + 3] < 128)
            {
                int nearest = valid[0];
                for (unsigned i = 1; i < count; ++i)
                    if (std::abs(valid[i] - x) < std::abs(nearest - x))
                        nearest = valid[i];
                std::copy_n(original.data() + 4 * nearest, 3, row + 4 * x);
            }
            row[4 * x + 3] = 255;
        }
    }
    bool any = false;
    for (auto row : rows)
        any = any || row >= 0;
    if (!any)
        return false;
    for (int y = 0; y < 64; ++y)
        if (rows[y] < 0)
        {
            int nearest = -1;
            for (int i = 0; i < 64; ++i)
                if (rows[i] >= 0 && (nearest < 0 || std::abs(i - y) < std::abs(nearest - y)))
                    nearest = i;
            std::copy_n(pixels.data() + 4 * ((448 + nearest) * 512 + 256), 1024, pixels.data() + 4 * ((448 + y) * 512 + 256));
        }
    return true;
}

void KvExtendRegion(std::vector<unsigned char>& pixels, int left, int top, int width, int height, int maxGap = 8)
{
    // Only fill the gaps of a real garment. A piece that covers just a part of the region (tattoo lines, a bare torso with a belt)
    // must leave the skin showing, filling it smeared the lines over the whole body.
    long covered = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (pixels[4 * ((top + y) * 512 + left + x) + 3] >= 160)
                ++covered;
    if (covered * 100 < static_cast<long>(width) * height * 40)
        return;
    std::vector<int> columns(width, -1);
    for (int x = 0; x < width; ++x)
    {
        std::vector<int> rows;
        for (int y = 0; y < height; ++y)
            if (pixels[4 * ((top + y) * 512 + left + x) + 3] >= 160)
                rows.push_back(y);
        if (rows.empty())
            continue;
        columns[x] = x;
        for (int y = 0; y < height; ++y)
        {
            auto* dst = pixels.data() + 4 * ((top + y) * 512 + left + x);
            if (dst[3] >= 160)
                continue;
            int nearest = rows[0];
            for (auto row : rows)
                if (std::abs(row - y) < std::abs(nearest - y))
                    nearest = row;
            if (std::abs(nearest - y) > maxGap)
                continue;       // only small holes: a wide gap is bare skin between pieces (a belt must not grow over it)
            std::copy_n(pixels.data() + 4 * ((top + nearest) * 512 + left + x), 4, dst);
            dst[3] = 255;
        }
    }
    for (int x = 0; x < width; ++x)
        if (columns[x] < 0)
        {
            int nearest = -1;
            for (int i = 0; i < width; ++i)
                if (columns[i] >= 0 && (nearest < 0 || std::abs(i - x) < std::abs(nearest - x)))
                    nearest = i;
            if (nearest < 0)
                continue;
            for (int y = 0; y < height; ++y)
                std::copy_n(pixels.data() + 4 * ((top + y) * 512 + left + nearest), 4,
                    pixels.data() + 4 * ((top + y) * 512 + left + x));
        }
}

// The equipment atlas (512x512 RGBA) -> the race's native body atlas.
bool KvNativeBody(void* character, unsigned race, unsigned gender, unsigned skinValue, KvOutfit const& state,
    std::vector<unsigned char> const& outfit, std::vector<unsigned char> const& gloveless,
    std::vector<unsigned char>& result, unsigned& width, unsigned& height)
{
    auto const* map = KvMap(race, gender);
    if (!map)
    {
        KvLog("fitting map %u-%u missing", race, gender);
        return false;
    }
    char const* skinPath = nullptr;
    for (auto const& record : RetailSelections(race))
        if (record.gender == gender && record.kind == 1 && record.target == 11
            && record.choices[0] == (skinValue << 16) && record.choices[1] == 0xffffffffu && record.choices[2] == 0xffffffffu)
        {
            skinPath = record.path;
            break;
        }
    if (!skinPath)
    {
        KvLog("no body skin record for skin %u", skinValue);
        return false;
    }
    auto const* skin = KvSkin(skinPath);
    auto const& gear = state.gear;
    if (race == 61 && gear[5])
    {
        char bare[160]{};
        sprintf_s(bare, "custom\\equipment\\native\\legskin-v3-61-%u-%u.blp", gender, skinValue);
        if (auto const* detailed = KvSkin(bare))
            skin = detailed;
    }
    if (!skin || skin->width != map->width || skin->height != map->height)
    {
        KvLog("skin %s %s (%ux%u vs map %ux%u)", skinPath, skin ? "size mismatch" : "not in bank", skin ? skin->width : 0, skin ? skin->height : 0, map->width, map->height);
        return false;
    }
    std::vector<unsigned char> composed(skin->rgba);
    auto extended = outfit;
    if (!map->coverage.empty())
    {
        if (gear[3])
        {
            KvExtendRegion(extended, 256, 0, 256, 192);
            KvExtendRegion(extended, 0, 0, 256, 128);
        }
        if (gear[5])
            KvExtendRegion(extended, 256, 192, 256, 128);
        if (race == 61 && gear[6])
            KvExtendRegion(extended, 256, 320, 256, 128);
    }
    unsigned long long tintSum[3]{};
    unsigned tintCount = 0;
    if (race == 61 && gear[6])
        for (unsigned y = 448; y < 512; ++y)
            for (unsigned x = 256; x < 512; ++x)
            {
                auto* color = outfit.data() + 4 * (y * 512 + x);
                if (color[3] < 128)
                    continue;
                for (unsigned k = 0; k < 3; ++k)
                    tintSum[k] += color[k];
                ++tintCount;
            }
    for (unsigned y = 0; y < map->height; ++y)
        for (unsigned x = 0; x < map->width; ++x)
        {
            unsigned char* out = composed.data() + 4 * (y * map->width + x);
            out[3] = 255; // the body albedo alpha carries material data, not coverage
            unsigned sample = map->pixels[y * map->width + x];
            if (sample == 0xffffffffu)
                continue;
            if (race == 61 && !map->coverage.empty() && map->coverage[y * map->width + x] == 4 && gear[6])
            {
                auto* ankle = extended.data() + 4 * (447 * 512 + (sample & 65535u));
                if (ankle[3])
                    KvOver(out, ankle);
                continue;
            }
            if (race == 61 && ((sample >> 16) >= 448 || (!map->coverage.empty() && map->coverage[y * map->width + x] == 3)))
            {
                if (tintCount)
                {
                    unsigned shade = (unsigned(out[0]) + out[1] + out[2]) / 3;
                    for (unsigned k = 0; k < 3; ++k)
                        out[k] = static_cast<unsigned char>((std::min)(255ull,
                            (out[k] * 65ull + shade * (tintSum[k] / tintCount) * 35ull / 128ull) / 100ull));
                }
                continue;
            }
            auto const* layer = outfit.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            if (gear[8] && (sample & 65535u) < 256 && (sample >> 16) >= 128 && (sample >> 16) < 256)
            {
                float compression = race == 60 ? .82f : .78f;
                int gloveY = 256 - int((256 - int(sample >> 16)) / compression);
                if (gloveY >= 128)
                    layer = outfit.data() + 4 * (gloveY * 512 + (sample & 65535u));
                else
                    layer = gloveless.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            }
            if (!map->coverage.empty() && layer[3] < 160)
            {
                auto mask = map->coverage[y * map->width + x];
                if ((mask == 1 && (sample >> 16) < 128 && gear[3]) || (mask == 2 && gear[5]))
                    layer = extended.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            }
            if (layer[3])
                KvOver(out, layer);
        }
    width = map->width;
    height = map->height;
    result = std::move(composed);
    return true;
}

// Like HaranirLoadTexture, for the 1024x512 body atlas.
std::shared_ptr<void> KvLoadTexture(char const* path)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path, "rb"))
        return {};
    std::fseek(file, 0, SEEK_END);
    long length = std::ftell(file);
    std::rewind(file);
    std::vector<unsigned char> bytes(length > 0 ? length : 0);
    bool ok = length >= 1172 && std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    if (!ok || Field<unsigned>(bytes.data(), 0) != 0x32504C42 || bytes[8] != 3)
        return {};
    CTexture* ctexture = new (__FILE__, __LINE__) CTexture(); // same allocator as the engine's own textures (HandleClose deletes it)
    HandleCreate(ctexture, path);
    std::strncpy(ctexture->m_filename, path, 0x103);
    ctexture->m_filename[0x103] = 0;
    void* texture = ctexture;
    int32_t decoded = PumpBlpTextureAsync(ctexture, bytes.data());
    auto result = std::shared_ptr<void>(texture, [](void* value)
    {
        N_47BF30(value);
    });
    if (!decoded)
        return {};
    return result;
}

std::shared_ptr<void> KvWriteAndLoad(char const* family, std::vector<unsigned char> pixels, unsigned width,
    unsigned height)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (auto value : pixels)
        hash = (hash ^ value) * 1099511628211ull;
    char name[200]{};
    sprintf_s(name, "Interface\\AddOns\\EsteriaAppearanceCache\\KvEquipment\\%s-%016llx.blp", family,
        static_cast<unsigned long long>(hash));
    if (!WriteHaranirBlp(name, std::move(pixels), width, height, false))
        return {};
    char executable[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, executable, MAX_PATH))
        return {};
    char* slash = std::strrchr(executable, '\\');
    if (!slash)
        return {};
    slash[1] = 0;
    return KvLoadTexture((std::string(executable) + name).c_str());
}

void KvShowGeometry(void* instance, KvOutfit const& state, unsigned race, unsigned gender)
{
    auto show = N_82C7C0;
    show(instance, 20100, 20139, 0);
    show(instance, 20200, 20200, 0);
    show(instance, 20300, 20300, state.robe ? 0 : 1); // Aetheria anatomy split: legs / hands live here
    show(instance, 0, 0, 1);
    bool any = std::any_of(state.gear.begin(), state.gear.end(), [](unsigned v) { return v != 0; });
    if (race == 61 && (state.robe || any))
        show(instance, 3101, 3101, 1);
    for (unsigned geo : state.leg)
        if (geo)
            show(instance, geo, geo, 1);
    show(instance, 1302, 1302, state.robe ? 1 : 0);
    show(instance, 20002, 20034, 0);
    if (state.shoes)
        show(instance, 20010 + state.bootStyle, 20010 + state.bootStyle, 1);
    if (state.greaves)
        show(instance, 20020 + state.bootStyle, 20020 + state.bootStyle, 1);
    if (state.gloves)
        show(instance, 20030 + state.gloveStyle, 20030 + state.gloveStyle, 1);
    // The selected built-in outfit pieces give way to equipped armor; hair, wings and the base body stay.
    bool armor = false;
    for (unsigned slot = 0; slot <= 10; ++slot)
        armor = armor || state.gear[slot] != 0;
    if (armor)
    {
        auto hideGroup = [&](unsigned group) { show(instance, group * 100, group * 100 + 99, 0); };
        if (race == 60)
        {
            hideGroup(32);
            hideGroup(34);
            hideGroup(33);
        }
        else
        {
            hideGroup(35);
            if (gender)
                hideGroup(32);
            hideGroup(34);
            hideGroup(31);
            show(instance, 3101, 3101, 1);
        }
    }
}

// Returns true when the outfit is bound (or nothing needs binding).
bool KvBindOutfit(void* character)
{
    unsigned race = RaceOf(CC<unsigned>(character, 0x18));
    if (race != 60 && race != 61)
        return true;
    unsigned gender = CC<unsigned>(character, 0x1C);
    void* instance = CC<void*>(character, 0x38);
    if (!instance || gender > 1)
    {
        KvLog("bind: no model or bad sex instance=%p gender=%u", instance, gender);
        return false;
    }
    auto fields = HighmountainFields(character);
    KvOutfit next;
    next.fields = fields;
    for (unsigned i = 0; i < 12; ++i)
        next.gear[i] = CC<unsigned>(character, 0x428 + i * 4);
    auto& gear = next.gear;
    auto chest = KvItemGeoset(gear[3]);
    auto pants = KvItemGeoset(gear[5]);
    auto boots = KvItemGeoset(gear[6]);
    auto gloves = KvItemGeoset(gear[8]);
    auto shirt = KvItemGeoset(gear[2]);
    auto belt = KvItemGeoset(gear[4]);
    // Robes and skirts are worn as trousers by the Kyrian and the Venthyr: the item's leg layers are baked onto the legs, no skirt
    // is drawn (the skirt mesh rig of the Venthyr robe was tried and dropped).
    bool chestRobe = false;
    bool legSkirt = false;
    next.robe = chestRobe || legSkirt;
    bool const robeChest = gear[3] && chest[2] != 0;
    next.shoes = race == 60 && gear[6] && (CC<unsigned>(character, 0x1D0 + 7 * 0x3C + 0x38) & 1);
    next.greaves = race == 61 && gear[6];
    next.gloves = gear[8] != 0;
    next.gloveStyle = next.gloves ? (std::min)(3, (std::max)(0, gloves[0])) : 0;
    next.bootStyle = gear[6] ? (std::min)(4, (std::max)(0, boots[0])) : 0;
    next.leg[0] = !next.robe && pants[0] ? 20102u : 0;   // always the fitted trousers: the wide style (20104) read as a long robe
    next.leg[1] = !next.robe && !next.bootStyle && pants[1] ? (pants[1] > 1 ? 20113u : 20112u) : 0;
    next.leg[2] = !next.robe && gear[2] && shirt[1] ? 20122u : 0;
    next.leg[3] = 0;     // no belt mesh: it grew into a wide band on the Venthyr
    next.legMeshes = next.leg[0] || next.leg[1] || next.leg[2] || next.leg[3];

    KvLog("bind race=%u sex=%u fields=%u,%u,%u,%u,%u gear=%u,%u,%u,%u,%u,%u,%u,%u,%u robe=%d", race, gender, fields[0], fields[1], fields[2], fields[3], fields[4], gear[0], gear[1], gear[2], gear[3], gear[4], gear[5], gear[6], gear[7], gear[8], next.robe ? 1 : 0);
    auto const found = kvOutfits.find(character);
    if (found != kvOutfits.end() && found->second.texture && found->second.gear == next.gear
        && found->second.fields == next.fields && found->second.robe == next.robe && found->second.shoes == next.shoes
        && found->second.greaves == next.greaves && found->second.gloves == next.gloves
        && found->second.bootStyle == next.bootStyle && found->second.gloveStyle == next.gloveStyle)
    {
        auto& state = found->second;
        for (unsigned i = 0; i < 4; ++i)
            state.leg[i] = next.leg[i];
        state.legMeshes = next.legMeshes;
        N_825260(instance, 11, state.texture.get());
        KvShowGeometry(instance, state, race, gender);
        if ((state.robe || state.shoes || state.greaves || state.gloves || state.legMeshes) && state.garment)
            N_825260(instance, 15, state.garment.get());
        return true;
    }

    // Resolve every retained item display before reading the layers (stock only resolves dirty regions).
    unsigned dirty = CC<unsigned>(character, 0x0C);
    CC<unsigned>(character, 0x0C) = dirty | 0xFF;
    N_4ED6D0(character, 1);
    CC<unsigned>(character, 0x0C) = dirty;

    unsigned const counts[] = {2, 7, 1, 5, 7, 3, 6, 1};
    unsigned const rectangles[][4] = {{0, 0, 256, 128}, {0, 128, 256, 128}, {0, 256, 256, 64},
        {256, 0, 256, 128}, {256, 128, 256, 64}, {256, 192, 256, 128},
        {256, 320, 256, 128}, {256, 448, 256, 64}};
    std::vector<unsigned char> pixels(512 * 512 * 4);
    std::vector<unsigned char> gloveless(512 * 512 * 4);
    for (unsigned region = 0; region < 8; ++region)
    {
        unsigned base = 0x1D0 + region * 0x3C;
        unsigned mask = CC<unsigned>(character, base + 0x38);
        for (unsigned slot = 0; slot < counts[region]; ++slot)
        {
            if (!(mask & (1u << slot)))
                continue;
            unsigned display = CC<unsigned>(character, base + 0x1C + slot * 4);
            if (robeChest && region >= 5 && display == gear[3])
                continue;               // a robe chest piece brings its long skirt layers: here it dresses the torso and arms only, the legs wear the trousers
            if (next.robe && region == 6 && display == gear[6])
                continue;
            if (chestRobe && region == 5 && display == gear[5])
                continue;
            void* layer = CC<void*>(character, base + slot * 4);
            if (!layer)
                continue;
            if (!CE<void*>(layer, 0xAC) || !CE<std::uint16_t>(layer, 0x1C))
            {
                unsigned metadata[4]{};
                N_4F2E50(layer, metadata, 1);
            }
            auto* data = CE<unsigned char*>(layer, 0xAC);
            unsigned size = CE<unsigned>(layer, 0xB0) & 0xFFFFF;
            std::array<unsigned char, 4> probe{};
            if (!data || !ClothingPixel(data, size, 0, 0, probe))
            {
                KvLog("layer not readable region=%u slot=%u data=%p size=%u", region, slot, data, size);
                return false; // not loaded yet: the next refresh retries
            }
            unsigned width = Field<unsigned>(data, 12);
            unsigned height = Field<unsigned>(data, 16);
            auto const& rect = rectangles[region];
            bool glove = gear[8] && display == gear[8];
            for (unsigned y = 0; y < rect[3]; ++y)
                for (unsigned x = 0; x < rect[2]; ++x)
                {
                    std::array<unsigned char, 4> pixel{};
                    if (!ClothingPixel(data, size, x * width / rect[2], y * height / rect[3], pixel))
                        return false;
                    ClothingOver(pixels.data() + ((rect[1] + y) * 512 + rect[0] + x) * 4, pixel);
                    if (!glove)
                        ClothingOver(gloveless.data() + ((rect[1] + y) * 512 + rect[0] + x) * 4, pixel);
                }
        }
    }
    if (race == 60 && gear[6])
        KvSealBootFoot(pixels);

    std::shared_ptr<void> garment;
    if (next.robe || next.shoes || next.greaves || next.gloves || next.legMeshes)
    {
        auto plain = pixels;
        if (next.greaves && !next.robe)
        {
            auto sealed = plain;
            KvExtendRegion(sealed, 256, 320, 256, 128);
            for (unsigned y = 420; y < 448; ++y)
                std::copy_n(sealed.data() + 4 * (y * 512 + 256), 256 * 4, plain.data() + 4 * (y * 512 + 256));
        }
        garment = KvWriteAndLoad(race == 60 ? "garment-60" : "garment-61", std::move(plain), 512, 512);
        if (!garment)
            return false;
    }
    unsigned skinValue = 0;
    {
        auto const& options = race == 60
            ? (gender ? CreatureAppearance::KyrianFemaleControls : CreatureAppearance::KyrianMaleControls)
            : (gender ? CreatureAppearance::VenthyrFemaleControls : CreatureAppearance::VenthyrMaleControls);
        skinValue = (fields[options[0].field] / options[0].factor) % options[0].count;
    }
    std::vector<unsigned char> body;
    unsigned width = 0;
    unsigned height = 0;
    if (!KvNativeBody(character, race, gender, skinValue, next, pixels, gloveless, body, width, height))
    {
        KvLog("native body fit failed skin=%u", skinValue);
        return false;
    }
    auto texture = KvWriteAndLoad(race == 60 ? (gender ? "native-60-1" : "native-60-0")
        : (gender ? "native-61-1" : "native-61-0"), std::move(body), width, height);
    if (!texture)
    {
        KvLog("body texture write/load failed");
        return false;
    }
    KvLog("bound body texture %ux%u", width, height);
    next.texture = texture;
    next.garment = garment;
    auto& state = kvOutfits[character] = next;
    N_825260(instance, 11, state.texture.get());
    KvShowGeometry(instance, state, race, gender);
    if (garment)
        N_825260(instance, 15, state.garment.get());
    return true;
}


// Slime Giant (Esteria id 65): like Esteria, worn armor is a clothing overlay on the skeleton (slime_clothing.py, geoset 6901, texture
// type 14) whose UVs lie in the arms / torso sections of the equipment atlas; the bare colour skin stays on texture type 11.
std::unordered_map<void*, std::pair<unsigned long long, std::shared_ptr<void>>> kvSlime;
std::unordered_map<void*, std::shared_ptr<void>> kvSlimeCloth;

bool KvSlimeBind(void* character)
{
    if (RaceOf(CC<unsigned>(character, 0x18)) != 65)
        return true;
    void* instance = CC<void*>(character, 0x38);
    if (!instance)
    {
        kvSlime.erase(character);
        kvSlimeCloth.erase(character);
        return true;
    }
    constexpr unsigned overlay = 6901;
    // the bare colour skin is bound through the same path on every refresh (a Slime without armor looked pale and flat otherwise)
    auto fields = HighmountainFields(character);
    unsigned color = fields[0] % CreatureAppearance::SlimeControls[0].count;
    std::string path = color < 5 ? "custom\\slimegiant\\unknown\\" + std::to_string(3491663 + 2 * color) + ".blp"
        : "custom\\slimegiant\\colors\\" + std::to_string(2 * (color - 5)) + ".blp";
    auto const* skin = KvSkin(path.c_str());
    if (!skin)
    {
        KvLog("slime: skin %s not in bank", path.c_str());
        return false;
    }
    unsigned dirty = CC<unsigned>(character, 0x0C);
    CC<unsigned>(character, 0x0C) = dirty | 0xFF;
    N_4ED6D0(character, 1);
    CC<unsigned>(character, 0x0C) = dirty;
    unsigned const rectangles[][4] = {{0, 0, 256, 128}, {0, 128, 256, 128}, {0, 256, 256, 64}, {256, 0, 256, 128},
        {256, 128, 256, 64}, {256, 192, 256, 128}, {256, 320, 256, 128}, {256, 448, 256, 64}};
    unsigned const counts[] = {2, 7, 1, 5, 7, 3, 6, 1};
    std::vector<unsigned char> atlas(512 * 512 * 4);
    bool any = false;
    std::uint64_t hash = 14695981039346656037ull ^ color;
    for (unsigned region = 0; region < 8; ++region)
    {
        unsigned base = 0x1D0 + region * 0x3C;
        unsigned mask = CC<unsigned>(character, base + 0x38);
        for (unsigned slot = 0; slot < counts[region]; ++slot)
        {
            if (!(mask & (1u << slot)))
                continue;
            void* layer = CC<void*>(character, base + slot * 4);
            if (!layer)
                continue;
            if (!CE<void*>(layer, 0xAC) || !CE<std::uint16_t>(layer, 0x1C))
            {
                unsigned metadata[4]{};
                N_4F2E50(layer, metadata, 1);
            }
            auto* data = CE<unsigned char*>(layer, 0xAC);
            unsigned size = CE<unsigned>(layer, 0xB0) & 0xFFFFF;
            std::array<unsigned char, 4> probe{};
            if (!data || !ClothingPixel(data, size, 0, 0, probe))
                return false;
            unsigned width = Field<unsigned>(data, 12);
            unsigned height = Field<unsigned>(data, 16);
            auto const& rect = rectangles[region];
            for (unsigned y = 0; y < rect[3]; ++y)
                for (unsigned x = 0; x < rect[2]; ++x)
                {
                    std::array<unsigned char, 4> pixel{};
                    if (!ClothingPixel(data, size, x * width / rect[2], y * height / rect[3], pixel))
                        return false;
                    ClothingOver(atlas.data() + ((rect[1] + y) * 512 + rect[0] + x) * 4, pixel);
                }
            any = true;
            hash = (hash ^ (region * 16 + slot)) * 1099511628211ull ^ CC<unsigned>(character, base + 0x1C + slot * 4);
        }
    }
    auto found = kvSlime.find(character);
    auto cloth = kvSlimeCloth.find(character);
    if (found != kvSlime.end() && found->second.first == hash && found->second.second)
    {
        N_825260(instance, 11, found->second.second.get());
        if (any && cloth != kvSlimeCloth.end() && cloth->second)
            N_825260(instance, 14, cloth->second.get());
        N_82C7C0(instance, overlay, overlay, any ? 1 : 0);
        return true;
    }
    auto texture = KvWriteAndLoad("slime-native", std::vector<unsigned char>(skin->rgba), skin->width, skin->height);
    if (!texture)
    {
        KvLog("slime: body texture write/load failed");
        return false;
    }
    N_825260(instance, 11, texture.get());
    kvSlimeCloth.erase(character);
    if (any)
    {
        auto clothing = KvWriteAndLoad("slime-clothing", std::move(atlas), 512, 512);
        if (!clothing)
        {
            KvLog("slime: clothing texture write/load failed");
            return false;
        }
        N_825260(instance, 14, clothing.get());
        kvSlimeCloth[character] = clothing;
    }
    N_82C7C0(instance, overlay, overlay, any ? 1 : 0);
    kvSlime[character] = {hash, texture};
    KvLog("slime: bound colour %u armor %d", color, any ? 1 : 0);
    return true;
}


void KvBlendAtlas(std::vector<unsigned char>& body, KvFittingMap const* map, std::vector<unsigned char> const& atlas, bool sharp = false, int blurRadius = 2, int blurPasses = 2)
{
    if (map && sharp)
    {
        for (unsigned i = 0; i < map->width * map->height; ++i)
        {
            unsigned sample = map->pixels[i];
            if (sample == 0xffffffffu)
                continue;
            auto const* pixel = atlas.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            if (pixel[3])
                KvOver(body.data() + 4 * i, pixel);
        }
        return;
    }
        // the armor atlas regions are 256 wide and get stretched over the body skin: map into a premultiplied layer and soften
        // it with a small blur so the atlas pixels do not show as squares
        std::size_t const w = map->width, h = map->height;
        std::vector<float> layer(w * h * 4, 0.f), tmp(w * h * 4, 0.f);
        for (std::size_t i = 0; i < w * h; ++i)
        {
            unsigned sample = map->pixels[i];
            if (sample == 0xffffffffu)
                continue;
            auto const* pixel = atlas.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            if (!pixel[3])
                continue;
            float a = pixel[3] / 255.0f;
            layer[4 * i] = pixel[0] * a;
            layer[4 * i + 1] = pixel[1] * a;
            layer[4 * i + 2] = pixel[2] * a;
            layer[4 * i + 3] = a;
        }
        int const radius = blurRadius;
        for (int pass = 0; pass < blurPasses; ++pass)
        {
            for (std::size_t y = 0; y < h; ++y)
                for (std::size_t x = 0; x < w; ++x)
                {
                    float sum[4] = {0, 0, 0, 0};
                    int count = 0;
                    for (int d = -radius; d <= radius; ++d)
                    {
                        long xx = static_cast<long>(x) + d;
                        if (xx < 0 || xx >= static_cast<long>(w))
                            continue;
                        for (int c = 0; c < 4; ++c)
                            sum[c] += layer[4 * (y * w + xx) + c];
                        ++count;
                    }
                    for (int c = 0; c < 4; ++c)
                        tmp[4 * (y * w + x) + c] = sum[c] / count;
                }
            for (std::size_t y = 0; y < h; ++y)
                for (std::size_t x = 0; x < w; ++x)
                {
                    float sum[4] = {0, 0, 0, 0};
                    int count = 0;
                    for (int d = -radius; d <= radius; ++d)
                    {
                        long yy = static_cast<long>(y) + d;
                        if (yy < 0 || yy >= static_cast<long>(h))
                            continue;
                        for (int c = 0; c < 4; ++c)
                            sum[c] += tmp[4 * (yy * w + x) + c];
                        ++count;
                    }
                    for (int c = 0; c < 4; ++c)
                        layer[4 * (y * w + x) + c] = sum[c] / count;
                }
        }
        for (std::size_t i = 0; i < w * h; ++i)
        {
            float a = layer[4 * i + 3];
            if (a < 0.02f)
                continue;
            unsigned char pixel[4];
            for (int c = 0; c < 3; ++c)
                pixel[c] = static_cast<unsigned char>((std::min)(255.0f, layer[4 * i + c] / a + 0.5f));
            pixel[3] = static_cast<unsigned char>((std::min)(255.0f, a * 255.0f + 0.5f));
            KvOver(body.data() + 4 * i, pixel);
        }
}

// Centaurs (Esteria id 68): every worn layer of the player equipment atlas is fitted onto the 1024x512 body skin by the map made in
// centaur_fit_map.py (arms and torso like a Kul Tiran, the pants and boots wrapped around the barrel, legs and hooves).
std::unordered_map<void*, std::pair<unsigned long long, std::shared_ptr<void>>> kvCentaur;
std::unordered_map<void*, std::shared_ptr<void>> kvCentaurTop;      // the female's top texture (type 12), armor drawn into it

bool KvCentaurBind(void* character)
{
    if (RaceOf(CC<unsigned>(character, 0x18)) != 68)
        return true;
    void* instance = CC<void*>(character, 0x38);
    unsigned gender = CC<unsigned>(character, 0x1C);
    if (!instance || gender > 1)
        return true;
    bool worn = false;
    for (unsigned i = 0; i < 12; ++i)
        worn = worn || CC<unsigned>(character, 0x428 + i * 4);
    constexpr unsigned overlay = 6901; // the clothing overlay (centaur_clothing.py), drawn with the replaceable texture type 14
    if (!worn)
    {
        kvCentaur.erase(character);
        N_82C7C0(instance, overlay, overlay, 0);
        return true; // the plain colour skin the selection bound stays
    }
    unsigned dirty = CC<unsigned>(character, 0x0C);
    CC<unsigned>(character, 0x0C) = dirty | 0xFF;
    N_4ED6D0(character, 1);
    CC<unsigned>(character, 0x0C) = dirty;
    unsigned const counts[] = {2, 7, 1, 5, 7, 3, 6, 1};
    unsigned const rectangles[][4] = {{0, 0, 256, 128}, {0, 128, 256, 128}, {0, 256, 256, 64},
        {256, 0, 256, 128}, {256, 128, 256, 64}, {256, 192, 256, 128},
        {256, 320, 256, 128}, {256, 448, 256, 64}};
    // Esteria: boots paint only the hooves (section 7); the trousers keep their leg regions
    unsigned const boots = CC<unsigned>(character, 0x428 + 6 * 4);
    bool hasFoot = false;
    for (unsigned slot = 0; slot < counts[7]; ++slot)
        if ((CC<unsigned>(character, 0x1D0 + 7 * 0x3C + 0x38) & (1u << slot)) && boots
            && CC<unsigned>(character, 0x1D0 + 7 * 0x3C + 0x1C + slot * 4) == boots)
            hasFoot = true;
    std::vector<unsigned char> atlas(512 * 512 * 4);
    std::uint64_t hash = 14695981039346656037ull ^ (std::uint64_t(gender) << 8);
    for (unsigned region = 0; region < 8; ++region)
    {
        unsigned base = 0x1D0 + region * 0x3C;
        unsigned mask = CC<unsigned>(character, base + 0x38);
        for (unsigned slot = 0; slot < counts[region]; ++slot)
        {
            if (!(mask & (1u << slot)))
                continue;
            void* layer = CC<void*>(character, base + slot * 4);
            if (!layer)
                continue;
            unsigned target = region;
            if (region == 6 && boots && CC<unsigned>(character, base + 0x1C + slot * 4) == boots)
            {
                if (hasFoot)
                    continue;
                target = 7;
            }
            if (!CE<void*>(layer, 0xAC) || !CE<std::uint16_t>(layer, 0x1C))
            {
                unsigned metadata[4]{};
                N_4F2E50(layer, metadata, 1);
            }
            auto* data = CE<unsigned char*>(layer, 0xAC);
            unsigned size = CE<unsigned>(layer, 0xB0) & 0xFFFFF;
            std::array<unsigned char, 4> probe{};
            if (!data || !ClothingPixel(data, size, 0, 0, probe))
                return false; // not loaded yet: the next refresh retries
            unsigned width = Field<unsigned>(data, 12);
            unsigned height = Field<unsigned>(data, 16);
            auto const& rect = rectangles[target];
            for (unsigned y = 0; y < rect[3]; ++y)
                for (unsigned x = 0; x < rect[2]; ++x)
                {
                    std::array<unsigned char, 4> pixel{};
                    if (!ClothingPixel(data, size, x * width / rect[2], y * height / rect[3], pixel))
                        return false;
                    ClothingOver(atlas.data() + ((rect[1] + y) * 512 + rect[0] + x) * 4, pixel);
                }
            hash = (hash ^ (region * 16 + slot)) * 1099511628211ull ^ CC<unsigned>(character, base + 0x1C + slot * 4);
        }
    }
    auto found = kvCentaur.find(character);
    if (found != kvCentaur.end() && found->second.first == hash && found->second.second)
    {
        N_825260(instance, 14, found->second.second.get());
        N_82C7C0(instance, overlay, overlay, 1);
        return true;
    }
    auto texture = KvWriteAndLoad(gender ? "centaur-clothing-1" : "centaur-clothing-0", std::move(atlas), 512, 512);
    if (!texture)
    {
        KvLog("centaur: clothing texture write/load failed");
        return false;
    }
    N_825260(instance, 14, texture.get());
    N_82C7C0(instance, overlay, overlay, 1);
    kvCentaur[character] = {hash, texture};
    KvLog("centaur: clothing bound sex %u", gender);
    return true;
}


// Hozen (Esteria id 73): the Centaur's atlas fit on one male body (hozen_fit_map.py)
std::unordered_map<void*, std::pair<unsigned long long, std::shared_ptr<void>>> kvNative;

bool KvNativeBind(void* character, unsigned race, unsigned skinCount, unsigned markings, bool singleBody)
{
    if (RaceOf(CC<unsigned>(character, 0x18)) != race)
        return true;
    void* instance = CC<void*>(character, 0x38);
    unsigned gender = singleBody ? 0u : CC<unsigned>(character, 0x1C);
    if (gender > 1)
        return true;
    if (!instance)
        return true;
    bool worn = false;
    for (unsigned i = 0; i < 12; ++i)
        worn = worn || CC<unsigned>(character, 0x428 + i * 4);
    auto fields = HighmountainFields(character);
    unsigned skinValue = fields[0] % skinCount;
    char const* skinPath = nullptr;
    for (auto const& record : RetailSelections(race))
        if (record.gender == gender && record.kind == 1 && record.target == 11
            && record.choices[0] == (skinValue << 16) && record.choices[1] == 0xffffffffu && record.choices[2] == 0xffffffffu)
        {
            skinPath = record.path;
            break;
        }
    auto const* skin = skinPath ? KvSkin(skinPath) : nullptr;
    auto const* map = KvMap(race, gender);
    unsigned marking = markings ? fields[1] % markings : 0;
    if (!worn && !marking)
    {
        kvNative.erase(character);
        if (race == 77 || race == 78)
        {
            // pack races: bind the plain skin / face / hair ourselves on every refresh, the selection pass alone left them unbound in the world
            auto const& options = CreatureAppearance::Options(race, CC<unsigned>(character, 0x1C));
            unsigned realGender = CC<unsigned>(character, 0x1C);
            for (auto const& record : RetailSelections(race))
            {
                if (record.gender != realGender || record.kind != 1 || record.target < 11 || record.target > 13)
                    continue;
                bool selected = true;
                for (unsigned selector : record.choices)
                {
                    if (selector == 0xffffffff)
                        continue;
                    unsigned index = selector & 0xffff;
                    if (index >= options.size() || (fields[options[index].field] / options[index].factor) % options[index].count != selector >> 16)
                    {
                        selected = false;
                        break;
                    }
                }
                if (!selected)
                    continue;
                void* texture = N_4E8D30(record.path, nullptr);
                KvLog("pack: race %u gender %u slot %u %s %s", race, realGender, record.target, record.path, texture ? "bound" : "NOT FOUND");
                if (texture)
                {
                    N_825260(instance, record.target, texture);
                    N_47BF30(texture);
                }
            }
        }
        return true; // the plain colour skin the selection bound stays
    }
    if (!skin || !map || skin->width != map->width || skin->height != map->height)
    {
        KvLog("native: skin %s %s, map %s", skinPath ? skinPath : "(none)", skin ? "ok" : "not in bank", map ? "ok" : "missing");
        return false;
    }
    unsigned dirty = CC<unsigned>(character, 0x0C);
    CC<unsigned>(character, 0x0C) = dirty | 0xFF;
    N_4ED6D0(character, 1);
    CC<unsigned>(character, 0x0C) = dirty;
    unsigned const counts[] = {2, 7, 1, 5, 7, 3, 6, 1};
    unsigned const rectangles[][4] = {{0, 0, 256, 128}, {0, 128, 256, 128}, {0, 256, 256, 64},
        {256, 0, 256, 128}, {256, 128, 256, 64}, {256, 192, 256, 128},
        {256, 320, 256, 128}, {256, 448, 256, 64}};
    std::vector<unsigned char> atlas(512 * 512 * 4);
    std::uint64_t hash = 14695981039346656037ull ^ (skinValue + 1) ^ (std::uint64_t(gender) << 8) ^ (std::uint64_t(marking) << 16);
    for (unsigned region = 0; region < 8; ++region)
    {
        unsigned base = 0x1D0 + region * 0x3C;
        unsigned mask = CC<unsigned>(character, base + 0x38);
        for (unsigned slot = 0; slot < counts[region]; ++slot)
        {
            if (!(mask & (1u << slot)))
                continue;
            void* layer = CC<void*>(character, base + slot * 4);
            if (!layer)
                continue;
            if (!CE<void*>(layer, 0xAC) || !CE<std::uint16_t>(layer, 0x1C))
            {
                unsigned metadata[4]{};
                N_4F2E50(layer, metadata, 1);
            }
            auto* data = CE<unsigned char*>(layer, 0xAC);
            unsigned size = CE<unsigned>(layer, 0xB0) & 0xFFFFF;
            std::array<unsigned char, 4> probe{};
            if (!data || !ClothingPixel(data, size, 0, 0, probe))
                return false; // not loaded yet: the next refresh retries
            unsigned width = Field<unsigned>(data, 12);
            unsigned height = Field<unsigned>(data, 16);
            auto const& rect = rectangles[region];
            for (unsigned y = 0; y < rect[3]; ++y)
                for (unsigned x = 0; x < rect[2]; ++x)
                {
                    std::array<unsigned char, 4> pixel{};
                    if (!ClothingPixel(data, size, x * width / rect[2], y * height / rect[3], pixel))
                        return false;
                    ClothingOver(atlas.data() + ((rect[1] + y) * 512 + rect[0] + x) * 4, pixel);
                }
            hash = (hash ^ (region * 16 + slot)) * 1099511628211ull ^ CC<unsigned>(character, base + 0x1C + slot * 4);
        }
    }
    // gaps in the worn pieces would show the bare body through the armor: fill them like the Kyrian / Venthyr do
    if (CC<unsigned>(character, 0x428 + 3 * 4))
    {
        KvExtendRegion(atlas, 256, 0, 256, 192, 40);
        KvExtendRegion(atlas, 0, 0, 256, 128, 40);
    }
    if (CC<unsigned>(character, 0x428 + 5 * 4))
        KvExtendRegion(atlas, 256, 192, 256, 128, 40);
    auto found = kvNative.find(character);
    if (found != kvNative.end() && found->second.first == hash && found->second.second)
    {
        N_825260(instance, 11, found->second.second.get());
        return true;
    }
    std::vector<unsigned char> body(skin->rgba);
    if (worn && (race == 76 || race == 77 || race == 78))
    {
        // the Ethereal's atlas is only 1:1 with its texture: no smoothing, sharp pixels
        for (unsigned i = 0; i < map->width * map->height; ++i)
        {
            unsigned sample = map->pixels[i];
            if (sample == 0xffffffffu)
                continue;
            auto const* pixel = atlas.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            if (pixel[3])
                KvOver(body.data() + 4 * i, pixel);
        }
    }
    else if (worn)
    {
        // the armor atlas regions are 256 wide and get stretched over the body skin: map into a premultiplied layer and soften
        // it with a small blur so the atlas pixels do not show as squares
        std::size_t const w = map->width, h = map->height;
        std::vector<float> layer(w * h * 4, 0.f), tmp(w * h * 4, 0.f);
        for (std::size_t i = 0; i < w * h; ++i)
        {
            unsigned sample = map->pixels[i];
            if (sample == 0xffffffffu)
                continue;
            auto const* pixel = atlas.data() + 4 * ((sample >> 16) * 512 + (sample & 65535u));
            if (!pixel[3])
                continue;
            float a = pixel[3] / 255.0f;
            layer[4 * i] = pixel[0] * a;
            layer[4 * i + 1] = pixel[1] * a;
            layer[4 * i + 2] = pixel[2] * a;
            layer[4 * i + 3] = a;
        }
        int const radius = 2;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (std::size_t y = 0; y < h; ++y)
                for (std::size_t x = 0; x < w; ++x)
                {
                    float sum[4] = {0, 0, 0, 0};
                    int count = 0;
                    for (int d = -radius; d <= radius; ++d)
                    {
                        long xx = static_cast<long>(x) + d;
                        if (xx < 0 || xx >= static_cast<long>(w))
                            continue;
                        for (int c = 0; c < 4; ++c)
                            sum[c] += layer[4 * (y * w + xx) + c];
                        ++count;
                    }
                    for (int c = 0; c < 4; ++c)
                        tmp[4 * (y * w + x) + c] = sum[c] / count;
                }
            for (std::size_t y = 0; y < h; ++y)
                for (std::size_t x = 0; x < w; ++x)
                {
                    float sum[4] = {0, 0, 0, 0};
                    int count = 0;
                    for (int d = -radius; d <= radius; ++d)
                    {
                        long yy = static_cast<long>(y) + d;
                        if (yy < 0 || yy >= static_cast<long>(h))
                            continue;
                        for (int c = 0; c < 4; ++c)
                            sum[c] += tmp[4 * (yy * w + x) + c];
                        ++count;
                    }
                    for (int c = 0; c < 4; ++c)
                        layer[4 * (y * w + x) + c] = sum[c] / count;
                }
        }
        for (std::size_t i = 0; i < w * h; ++i)
        {
            float a = layer[4 * i + 3];
            if (a < 0.02f)
                continue;
            unsigned char pixel[4];
            for (int c = 0; c < 3; ++c)
                pixel[c] = static_cast<unsigned char>((std::min)(255.0f, layer[4 * i + c] / a + 0.5f));
            pixel[3] = static_cast<unsigned char>((std::min)(255.0f, a * 255.0f + 0.5f));
            KvOver(body.data() + 4 * i, pixel);
        }
    }
    if (marking)
    {
        std::string markPath = "custom\\hozen\\unknown\\mark_" + std::to_string(marking - 1) + ".blp";
        auto const* mark = KvSkin(markPath.c_str());
        if (mark && mark->width == map->width && mark->height == map->height)
        {
            for (unsigned i = 0; i < map->width * map->height; ++i)
                if (mark->rgba[4 * i + 3])
                    KvOver(body.data() + 4 * i, mark->rgba.data() + 4 * i);
        }
        else
            KvLog("native: marking %u not in bank", marking);
    }
    auto texture = KvWriteAndLoad((std::string("native-") + std::to_string(race) + "-" + std::to_string(gender)).c_str(), std::move(body), map->width, map->height);
    if (!texture)
    {
        KvLog("native: body texture write/load failed");
        return false;
    }
    N_825260(instance, 11, texture.get());
    kvNative[character] = {hash, texture};
    KvLog("native: race %u outfit bound on skin %u sex %u", race, skinValue, gender);
    return true;
}

bool KvHozenBind(void* character) { return KvNativeBind(character, 73, 18, 11, true); }
bool KvVorkaiBind(void* character)
{
    return KvNativeBind(character, 74, 8, 0, false);
}
bool KvMantidBind(void* character) { return KvNativeBind(character, 75, 8, 0, true); }
bool KvEtherealBind(void* character) { return KvNativeBind(character, 76, 8, 0, true); }
bool KvBansheeBind(void* character) { return KvNativeBind(character, 77, 6, 0, true); }      
bool KvFacelessBind(void* character) { return KvNativeBind(character, 78, 9, 0, true); }
