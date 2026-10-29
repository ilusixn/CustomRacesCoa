// Wrath's indexed body compositor writes alpha=255, even for transparent input pixels.
// Reuse its selected, loaded item layers, but retain alpha in a separate render-only material.
bool ClothingPixel(unsigned char const* data, unsigned size, unsigned x, unsigned y,
    std::array<unsigned char, 4>& pixel)
{
    if (size < 1172 || Field<unsigned>(const_cast<unsigned char*>(data), 0) != 0x32504C42)
        return false;
    unsigned width = Field<unsigned>(const_cast<unsigned char*>(data), 12);
    unsigned height = Field<unsigned>(const_cast<unsigned char*>(data), 16);
    unsigned offset = Field<unsigned>(const_cast<unsigned char*>(data), 20);
    unsigned length = Field<unsigned>(const_cast<unsigned char*>(data), 84);
    if (!width || !height || width > 512 || height > 512 || x >= width || y >= height
        || offset > size || length > size - offset)
        return false;
    unsigned at = y * width + x;
    if (data[8] == 3 && length == width * height * 4)
    {
        auto* color = data + offset + at * 4;
        pixel = {color[2], color[1], color[0], color[3]};
        return true;
    }
    unsigned alpha = data[9];
    if (data[8] != 1 || (alpha != 0 && alpha != 1 && alpha != 4 && alpha != 8)
        || length != width * height + (width * height * alpha + 7) / 8)
        return false;
    auto* color = data + 148 + data[offset + at] * 4;
    unsigned opacity = 255;
    if (alpha)
    {
        unsigned mask = (1u << alpha) - 1;
        opacity = ((data[offset + width * height + at * alpha / 8] >> (at * alpha % 8)) & mask) * 255 / mask;
    }
    pixel = {color[2], color[1], color[0], static_cast<unsigned char>(opacity)};
    return true;
}

void ClothingOver(unsigned char* target, std::array<unsigned char, 4> const& source)
{
    unsigned alpha = source[3];
    unsigned prior = target[3];
    unsigned result = alpha * 255 + prior * (255 - alpha);
    for (unsigned channel = 0; channel < 3; ++channel)
        target[channel] = static_cast<unsigned char>(result
            ? (source[channel] * alpha * 255 + target[channel] * prior * (255 - alpha) + result / 2) / result : 0);
    target[3] = static_cast<unsigned char>((result + 127) / 255);
}

void SetClothingMaterial(void* character, HaranirImages& images, void* instance)
{
    // Resolve every retained item display before snapshotting its ready bits. Stock only resolves dirty regions.
    unsigned dirty = CC<unsigned>(character, 0x0C);
    CC<unsigned>(character, 0x0C) = dirty | 0xFF;
    N_4ED6D0(character, 1);
    CC<unsigned>(character, 0x0C) = dirty;
    // Native CCharComponent's eight equipment regions, ordered exactly as 0x004F09D0..0x004F0E40.
    unsigned const counts[] = {2, 7, 1, 5, 7, 3, 6, 1};
    unsigned const rectangles[][4] = {{0, 0, 256, 128}, {0, 128, 256, 128}, {0, 256, 256, 64},
        {256, 0, 256, 128}, {256, 128, 256, 64}, {256, 192, 256, 128},
        {256, 320, 256, 128}, {256, 448, 256, 64}};
    struct Layer
    {
        unsigned region;
        unsigned size;
        unsigned char const* data;
    };
    std::vector<Layer> layers;
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned region = 0; region < 8; ++region)
    {
        unsigned base = 0x1D0 + region * 0x3C;
        unsigned mask = CC<unsigned>(character, base + 0x38);
        unsigned first = 0;
        unsigned count = counts[region];
        bool robe = (CC<unsigned>(character, 8) & 0x20) != 0;
        if (region == 5 && robe)
        {
            first = 1;
            count = 2;
        }
        if (region == 6 && robe)
        {
            first = 1;
            count = 4;
        }
        for (unsigned slot = first; slot < count; ++slot)
        {
            if (!(mask & (1u << slot)))
                continue;
            void* layer = CC<void*>(character, base + slot * 4);
            if (!layer)
                continue;
            if (!CE<void*>(layer, 0xAC) || !CE<std::uint16_t>(layer, 0x1C))
            {
                unsigned metadata[4]{};
                // The native synchronous readiness path completes the callback before reading its buffer.
                N_4F2E50(layer, metadata, 1);
            }
            auto* data = CE<unsigned char*>(layer, 0xAC);
            unsigned size = CE<unsigned>(layer, 0xB0) & 0xFFFFF;
            std::array<unsigned char, 4> pixel{};
            if (!data || !ClothingPixel(data, size, 0, 0, pixel))
                continue; // Failed input stays transparent rather than hiding the underlying fur.
            hash = (hash ^ region) * 1099511628211ull;
            for (unsigned i = 0; i < size; ++i)
                hash = (hash ^ data[i]) * 1099511628211ull;
            layers.push_back({region, size, data});
        }
    }
    if (!images.clothingTexture || images.clothingHash != hash)
    {
        std::vector<unsigned char> pixels(512 * 512 * 4);
        for (auto const& layer : layers)
        {
            auto const& rect = rectangles[layer.region];
            unsigned width = Field<unsigned>(const_cast<unsigned char*>(layer.data), 12);
            unsigned height = Field<unsigned>(const_cast<unsigned char*>(layer.data), 16);
            for (unsigned y = 0; y < rect[3]; ++y)
                for (unsigned x = 0; x < rect[2]; ++x)
                {
                    std::array<unsigned char, 4> pixel{};
                    if (!ClothingPixel(layer.data, layer.size, x * width / rect[2], y * height / rect[3], pixel))
                        return;
                    ClothingOver(pixels.data() + ((rect[1] + y) * 512 + rect[0] + x) * 4, pixel);
                }
        }
        char name[160]{};
        sprintf_s(name, "Interface\\AddOns\\EsteriaAppearanceCache\\%s\\clothing-%016llx.blp",
            RetailCacheFamily(images.race), static_cast<unsigned long long>(hash));
        if (!WriteHaranirBlp(name, pixels, 512, 512, false))
            return;
        char executable[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, executable, MAX_PATH))
            return;
        char* slash = std::strrchr(executable, '\\');
        if (!slash)
            return;
        slash[1] = 0;
        auto texture = HaranirLoadTexture((std::string(executable) + name).c_str());
        if (!texture)
            return;
        images.clothingTexture = std::move(texture);
        images.clothingHash = hash;
    }
    N_825260(instance, 14, images.clothingTexture.get());
}
