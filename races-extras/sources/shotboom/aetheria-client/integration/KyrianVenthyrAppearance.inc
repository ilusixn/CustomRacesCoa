// Merge inside the existing CreatureAppearance namespace. Option and standard array types are prerequisites.
    inline constexpr std::array<Option, 5> KyrianMale = {{
        {0, 19, 1},
        {1, 15, 1},
        {2, 160, 1},
        {3, 110, 1},
        {4, 96, 1}, // Values 80..95 mark independently packed new palettes.
    }};

    inline constexpr std::array<Option, 5> KyrianFemale = {{
        {0, 23, 1},
        {1, 20, 1},
        {2, 95, 1},
        {3, 55, 1},
        {4, 96, 1},
    }};

    inline constexpr std::array<Option, 5> VenthyrMale = {{
        {0, 7, 1},
        {1, 108, 1},
        {2, 8, 1},
        {3, 7, 1},
        {4, 32, 1},
    }};

    inline constexpr std::array<Option, 5> VenthyrFemale = {{
        {0, 6, 1},
        {1, 216, 1},
        {2, 6, 1},
        {3, 6, 1},
        {4, 40, 1},
    }};

    inline constexpr std::array<Option, 8> KyrianMaleControls = {{
        {0, 19, 1}, {2, 10, 1}, {3, 17, 1}, {2, 16, 10},
        {1, 3, 1}, {1, 5, 3}, {4, 4, 1}, {4, 4, 4},
    }};
    inline constexpr std::array<char const*, 8> KyrianMaleLabels = {{"Body Palette","Armor Palette","Wing Palette","Hair Palette","Hair Style","Headwear","Outfit","Shoulders"}};

    inline constexpr std::array<Option, 8> KyrianFemaleControls = {{
        {0, 23, 1}, {2, 5, 1}, {3, 25, 1}, {2, 19, 5},
        {1, 4, 1}, {1, 5, 4}, {4, 4, 1}, {4, 4, 4},
    }};
    inline constexpr std::array<char const*, 8> KyrianFemaleLabels = {{"Body Palette","Armor Palette","Wing Palette","Hair Palette","Hair Style","Headwear","Outfit","Shoulders"}};

    // Legacy wire values retain their meaning; logical controls split hair from wings.
    template <std::size_t N>
    constexpr std::array<std::uint8_t, N> DecodeKyrian(unsigned sex, std::array<std::uint8_t, N> fields)
    {
        unsigned const armorCount = sex ? 5 : 10;
        if (fields[4] >= 80)
        {
            // New wire layout: armor/hair in byte 2, wings in byte 3.
            fields[4] -= 80;
            return fields;
        }
        unsigned const packed = fields[3];
        unsigned const oldCount = sex ? 8 : 12;
        unsigned const legacyHairCount = sex ? 2 : 7;
        unsigned const legacyPackedCount = sex ? 16 : 56;
        unsigned wing, hair;
        if (packed >= legacyPackedCount)
        {
            unsigned const extra = packed - legacyPackedCount;
            if (extra < 3 * legacyHairCount)
            {
                wing = 8 + extra % 3;
                hair = extra / 3;
            }
            else
            {
                wing = (extra - 3 * legacyHairCount) % 11;
                hair = legacyHairCount + (extra - 3 * legacyHairCount) / 11;
            }
        }
        else if (packed < oldCount)
        {
            wing = packed % 4;
            hair = packed / 4;
        }
        else if (sex || packed < 24)
        {
            wing = 4 + (packed - oldCount) % 4;
            hair = (packed - oldCount) / 4;
        }
        else
        {
            wing = (packed - 24) % 8;
            hair = 3 + (packed - 24) / 8;
        }
        unsigned const theme = fields[4] / 16;
        if (theme > 0 && theme < 5)
        {
            wing = 3 + theme;
            if (!sex)
            {
                fields[0] = fields[2] = 2 + theme;
                hair = 2 + theme;
            }
        }
        fields[2] += armorCount * hair;
        fields[3] = wing;
        fields[4] %= 16;
        return fields;
    }

    template <std::size_t N>
    constexpr std::array<std::uint8_t, N> EncodeKyrian(unsigned sex, std::array<std::uint8_t, N> fields)
    {
        unsigned const armorCount = sex ? 5 : 10;
        unsigned const wing = fields[3];
        unsigned const hair = fields[2] / armorCount;
        if (wing >= 11 || hair >= (sex ? 5u : 10u))
        {
            fields[4] = 80 + fields[4] % 16;
            return fields;
        }
        fields[2] %= armorCount;
        unsigned const legacyHairCount = sex ? 2 : 7;
        if (wing >= 8 || hair >= legacyHairCount)
        {
            unsigned const extra = hair < legacyHairCount ? hair * 3 + wing - 8
                : 3 * legacyHairCount + (hair - legacyHairCount) * 11 + wing;
            fields[3] = (sex ? 16 : 56) + extra;
        }
        else if (wing < 4 && hair < (sex ? 2u : 3u))
            fields[3] = wing + 4 * hair;
        else if (sex || hair < 3)
            fields[3] = (sex ? 8 : 12) + 4 * hair + wing - 4;
        else
            fields[3] = 24 + 8 * (hair - 3) + wing;
        fields[4] %= 16;
        return fields;
    }

    inline constexpr std::array<Option, 8> VenthyrMaleControls = {{
        {0, 7, 1},
        {2, 8, 1},
        {3, 7, 1},
        {1, 6, 1},
        {4, 8, 1},
        {4, 4, 8},
        {1, 6, 6},
        {1, 3, 36},
    }};
    inline constexpr std::array<char const*, 8> VenthyrMaleLabels = {{"Skin / Body Palette","Accessory Palette","Hair Color","Body / Outfit Style","Hair Style","Facial Hair","Collar / Robe","Headwear"}};

    inline constexpr std::array<Option, 8> VenthyrFemaleControls = {{
        {0, 6, 1},
        {2, 6, 1},
        {3, 6, 1},
        {1, 4, 1},
        {4, 8, 1},
        {4, 5, 8},
        {1, 6, 4},
        {1, 9, 24},
    }};
    inline constexpr std::array<char const*, 8> VenthyrFemaleLabels = {{"Skin / Body Palette","Accessory Palette","Hair Color","Body / Outfit Style","Hair Style","Earrings","Collar / Cape","Headwear"}};

