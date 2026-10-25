#include "CoA/Source/Modules/Graphics.h"
#include <cstring>

namespace {
    bool s_entityShadows = true;   // the stock bytes
    bool s_lodObjectCull = true;

    bool IsOne(const char* value) {
        return value && strcmp(value, "1") == 0;
    }
}

bool CoA::Graphics::OnEntityShadows(CVar*, const char*, const char* value, void*) {
    s_entityShadows = IsOne(value);
    return true;
}

bool CoA::Graphics::EntityShadows() {
    return s_entityShadows;
}

bool CoA::Graphics::OnLodObjectCull(CVar*, const char*, const char* value, void*) {
    s_lodObjectCull = IsOne(value);
    return true;
}

bool CoA::Graphics::LodObjectCull() {
    return s_lodObjectCull;
}
