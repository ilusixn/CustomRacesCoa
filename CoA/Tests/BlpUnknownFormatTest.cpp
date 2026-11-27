#include "CoA/Tests/TestMain.h"
#include "Engine/Source/BLPFile/blp.h"
#include "Engine/Source/Gx/Blit.h"
#include "Engine/Source/Images/MippedImg.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstring>
#include <vector>

namespace {
    // Header from the 2026-11-05 Scarlet Monastery crash dumps: DXT encoding, preferred format 11 (retail BC5).
    std::vector<uint8_t> MakeDxtBlp(uint8_t preferredFormat, uint32_t size) {
        uint32_t blocks = (size / 4) * (size / 4);
        std::vector<uint8_t> data(sizeof(BLPHeader) + blocks * 16, 0x5A);

        BLPHeader header = {};
        header.m_magic = BLP_MAGIC_BLP2;
        header.m_formatVersion = 1;
        header.m_colorEncoding = COLOR_DXT;
        header.m_alphaSize = ALPHA_0;
        header.m_preferredFormat = preferredFormat;
        header.m_hasMips = 0;
        header.m_width = size;
        header.m_height = size;
        header.m_mipOffsets[0] = sizeof(BLPHeader);
        header.m_mipSizes[0] = blocks * 16;
        std::memcpy(data.data(), &header, sizeof(header));

        return data;
    }
}

COA_TEST(BlpUnknownPreferredFormatFailsLockChain2Cleanly) {
    std::vector<uint8_t> data = MakeDxtBlp(12, 8);
    CBLPFile image;
    CHECK(image.Source(data.data()));

    MipBits* images = nullptr;
    CHECK(!image.LockChain2("unknown.blp", PIXEL_ARGB8888, images, 0, 1));

    if (images) {
        SMemFree(images, __FILE__, __LINE__, 0);
    }
}

namespace {
    // Red: endpoints 255/0 (8-value mode), indices 0,1,2,... Green: endpoints 10/20 (6-value mode), indices 6,7,0,...
    const uint8_t BC5_BLOCK[16] = {
        255, 0, 0x88, 0xC6, 0xFA, 0x88, 0xC6, 0xFA,
        10, 20, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    void CheckBc5Pixels(const CImVector* px, uint32_t count) {
        const uint8_t red[8] = { 255, 0, 219, 182, 146, 109, 73, 36 };

        for (uint32_t i = 0; i < count; i++) {
            CHECK(px[i].r == red[i % 8]);
            CHECK(px[i].b == 0);
            CHECK(px[i].a == 0xFF);
        }

        CHECK(px[0].g == 0);
        CHECK(count < 2 || px[1].g == 255);
        CHECK(count < 3 || px[2].g == 10);
    }
}

COA_TEST(BlitDecodesBc5Block) {
    CImVector out[16] = {};
    C2iVector size = { 4, 4 };
    CHECK(CalcRowStride(BlitFormat_Bc5, 4, 4) == 16);
    CHECK(CalcRowStride(BlitFormat_Bc5, 5, 8) == 32);  // partial blocks count whole
    CHECK(CalcRowStride(BlitFormat_Bc5, 12, 8) == 48);
    CHECK(Blit(size, BlitAlpha_0, BC5_BLOCK, 16, BlitFormat_Bc5, out, 16, BlitFormat_Argb8888));
    CheckBc5Pixels(out, 16);
}

COA_TEST(BlitDecodesBc5SmallMipWithoutOverrun) {
    CImVector out[3] = {};
    out[2] = CImVector(1, 2, 3, 4);
    C2iVector size = { 2, 1 };
    CHECK(Blit(size, BlitAlpha_0, BC5_BLOCK, CalcRowStride(BlitFormat_Bc5, 2, 1), BlitFormat_Bc5, out, 8, BlitFormat_Argb8888));
    CheckBc5Pixels(out, 2);
    CHECK(out[2].b == 1 && out[2].a == 4);
}

COA_TEST(BlpBc5DecodesThroughLockChain2) {
    std::vector<uint8_t> data = MakeDxtBlp(PIXEL_BC5, 4);
    std::memcpy(data.data() + sizeof(BLPHeader), BC5_BLOCK, sizeof(BC5_BLOCK));
    CBLPFile image;
    CHECK(image.Source(data.data()));

    MipBits* images = nullptr;
    CHECK(image.LockChain2("bc5.blp", PIXEL_ARGB8888, images, 0, 1));

    if (images) {
        CheckBc5Pixels(reinterpret_cast<const CImVector*>(images[0].m_mip[0]), 16);
        SMemFree(images, __FILE__, __LINE__, 0);
    }
}

COA_TEST(BlpUnknownPreferredFormatFailsLockCleanly) {
    std::vector<uint8_t> data = MakeDxtBlp(255, 8);
    CBLPFile image;
    CHECK(image.Source(data.data()));

    uint8_t* bits = nullptr;
    uint32_t stride = 0;
    CHECK(!image.Lock(PIXEL_ARGB8888, 0, bits, stride));
}

COA_TEST(BlpKnownDxt5StillDecodes) {
    std::vector<uint8_t> data = MakeDxtBlp(PIXEL_DXT5, 8);
    CBLPFile image;
    CHECK(image.Source(data.data()));

    MipBits* images = nullptr;
    CHECK(image.LockChain2("dxt5.blp", PIXEL_ARGB8888, images, 0, 1));

    if (images) {
        SMemFree(images, __FILE__, __LINE__, 0);
    }
}

COA_TEST(BlitRejectsOutOfRangeFormats) {
    BlitFormat bad = static_cast<BlitFormat>(0xF0000000);
    CHECK(CalcRowStride(bad, 256, 256) == 0);
    CHECK(CalcRowStride(BlitFormats_Last, 256, 256) == 0);

    uint32_t in[16] = {};
    uint32_t out[16] = {};
    C2iVector size = { 4, 4 };
    CHECK(!Blit(size, BlitAlpha_0, in, 16, bad, out, 16, BlitFormat_Argb8888));
    CHECK(!Blit(size, BlitAlpha_0, in, 16, BlitFormat_Argb8888, out, 16, bad));
}
