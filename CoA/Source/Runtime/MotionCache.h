#pragma once

#include "CoA/Source/Runtime/RenderPipeline.h"
#include <cstddef>
#include <cstdint>

// Previous-frame bone palette and world transform of each animated model, keyed by the model
// pointer. The motion-vector pass reads last frame's data and stores this frame's.
namespace CoA::MotionCache {
    // Palette texture is 256 texels wide, 3 texels per bone: the most bone slots one skin section (batch) may use.
    constexpr uint32_t kMaxBones = 85;
    // The most bones of a whole model the cache keeps (character / creature models have well over kMaxBones).
    constexpr uint32_t kMaxModelBones = 512;

    struct Key {
        const void* model;
        uint32_t handle;
    };

    // Stores the model's data for frameIndex (the full bone array, by model bone index). False (nothing stored)
    // when count is 0 or > kMaxModelBones.
    bool Store(Key key, uint32_t frameIndex, const RenderPipeline::Mat4& world, const RenderPipeline::Mat4* bones, uint32_t count);

    // Data stored at frameIndex - 1 for this key; nullptr when absent, stored at another frame,
    // or the handle differs. Unaffected by a Store for frameIndex that already happened.
    const RenderPipeline::Mat4* Previous(Key key, uint32_t frameIndex, uint32_t& count);
    const RenderPipeline::Mat4* PreviousWorld(Key key, uint32_t frameIndex);

    // Drops models that were not stored at frameIndex (not drawn this frame).
    void EndFrame(uint32_t frameIndex);

    void Clear();
    size_t Size();
}
