#include "CoA/Source/Runtime/MotionCache.h"
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <vector>

namespace CoA::MotionCache {
    using RenderPipeline::Mat4;

    namespace {
        struct Slot {
            uint32_t frame;
            uint32_t count;     // 0 = empty
            Mat4 world;
            // Sized on first store and reused (grown only for a larger model), so no allocation per frame.
            std::vector<Mat4> bones;
        };

        // Two slots: the one written this frame and the one written last frame.
        struct Entry {
            uint32_t handle;
            Slot slots[2];
        };

        std::unordered_map<const void*, Entry>& Map() {
            static std::unordered_map<const void*, Entry> map;
            return map;
        }

        const Slot* FindPrevious(Key key, uint32_t frameIndex) {
            auto& map = Map();
            const auto it = map.find(key.model);
            if (it == map.end() || it->second.handle != key.handle) {
                return nullptr;
            }
            const uint32_t want = frameIndex - 1;
            for (const Slot& s : it->second.slots) {
                if (s.count != 0 && s.frame == want) {
                    return &s;
                }
            }
            return nullptr;
        }
    }

    bool Store(Key key, uint32_t frameIndex, const Mat4& world, const Mat4* bones, uint32_t count) {
        if (count == 0 || count > kMaxModelBones || !bones) {
            return false;
        }
        auto [it, inserted] = Map().try_emplace(key.model);
        Entry& e = it->second;
        if (inserted || e.handle != key.handle) {
            e.handle = key.handle;
            e.slots[0].count = 0;
            e.slots[1].count = 0;
        }
        // Reuse the slot already holding this frame, else an empty one, else the older one.
        Slot* slot = nullptr;
        for (Slot& s : e.slots) {
            if (s.count != 0 && s.frame == frameIndex) {
                slot = &s;
                break;
            }
        }
        if (!slot) {
            Slot& a = e.slots[0];
            Slot& b = e.slots[1];
            if (a.count == 0) {
                slot = &a;
            } else if (b.count == 0) {
                slot = &b;
            } else {
                slot = (static_cast<int32_t>(a.frame - b.frame) < 0) ? &a : &b;
            }
        }
        slot->frame = frameIndex;
        slot->count = count;
        slot->world = world;

        if (slot->bones.size() < count) {
            slot->bones.resize(count);
        }

        std::memcpy(slot->bones.data(), bones, sizeof(Mat4) * count);
        return true;
    }

    const Mat4* Previous(Key key, uint32_t frameIndex, uint32_t& count) {
        const Slot* s = FindPrevious(key, frameIndex);
        count = s ? s->count : 0;
        return s ? s->bones.data() : nullptr;
    }

    const Mat4* PreviousWorld(Key key, uint32_t frameIndex) {
        const Slot* s = FindPrevious(key, frameIndex);
        return s ? &s->world : nullptr;
    }

    void EndFrame(uint32_t frameIndex) {
        auto& map = Map();
        for (auto it = map.begin(); it != map.end();) {
            const Entry& e = it->second;
            const bool seen = (e.slots[0].count != 0 && e.slots[0].frame == frameIndex) ||
                              (e.slots[1].count != 0 && e.slots[1].frame == frameIndex);
            it = seen ? std::next(it) : map.erase(it);
        }
    }

    void Clear() {
        Map().clear();
    }

    size_t Size() {
        return Map().size();
    }
}
