#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <type_traits>
#include <unordered_map>

namespace {
    struct Store {
        std::unordered_map<std::string, int32_t> ints;
        std::unordered_map<std::string, bool> bools;
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, float> rates;
        std::unordered_map<std::string, std::vector<int32_t>> intVectors;
        std::unordered_map<std::string, std::vector<float>> floatVectors;
    };

    Store s_store;

    template <class T>
    T ReadValue(CDataStore* msg) {
        if constexpr (std::is_same_v<T, bool>) {
            return CoA::Read::U8(msg) != 0;
        } else if constexpr (std::is_same_v<T, float>) {
            return CoA::Read::F32(msg);
        } else {
            return CoA::Read::I32(msg);
        }
    }

    template <class T, class Fire>
    void ReadScalars(CDataStore* msg, std::unordered_map<std::string, T>& map, Fire fire) {
        const std::unordered_map<std::string, T> previous = map;
        map.clear();

        for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
            std::string name = CoA::Read::Name(msg);
            T value = ReadValue<T>(msg);

            if (CoA::Read::Failed(msg)) {
                break;
            }

            map[name] = value;

            if (!CoA::ClientState::InWorld()) {
                continue;
            }

            auto it = previous.find(name);

            if (it != previous.end() && it->second == value) {
                continue;
            }

            fire(name, it != previous.end() ? it->second : T {}, value);
        }
    }

    template <class T>
    void ReadVectors(CDataStore* msg, std::unordered_map<std::string, std::vector<T>>& map) {
        for (uint32_t n = CoA::Read::U32(msg); n && !CoA::Read::Failed(msg); --n) {
            std::string name = CoA::Read::Name(msg);
            std::vector<T> values;

            for (uint32_t k = CoA::Read::U32(msg); k && !CoA::Read::Failed(msg); --k) {
                T value = ReadValue<T>(msg);

                if (!CoA::Read::Failed(msg)) {
                    values.push_back(value);
                }
            }

            if (CoA::Read::Failed(msg)) {
                break;
            }

            map[name] = values;
        }
    }

    template <class M>
    const typename M::mapped_type* Find(const M& map, const char* name) {
        auto it = map.find(name);
        return it != map.end() ? &it->second : nullptr;
    }

    template <class T>
    int GetScalar(lua_State* L, const T* (*lookup)(const char*)) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        const T* value = lookup(name.c_str());

        if (!value) {
            lua_pushnil(L);
        } else if constexpr (std::is_same_v<T, bool>) {
            CoA::Script::PushBool(L, *value);
        } else if constexpr (std::is_same_v<T, float>) {
            CoA::Script::PushNum(L, *value);
        } else {
            CoA::Script::PushInt(L, *value);
        }

        return 1;
    }

    int GetIntConfig(lua_State* L) {
        return GetScalar<int32_t>(L, CoA::Config::Int);
    }

    int GetBoolConfig(lua_State* L) {
        return GetScalar<bool>(L, CoA::Config::Bool);
    }

    int GetFloatConfig(lua_State* L) {
        return GetScalar<float>(L, CoA::Config::Float);
    }

    int GetRateConfig(lua_State* L) {
        return GetScalar<float>(L, CoA::Config::Rate);
    }

    template <class T>
    int GetVector(lua_State* L, const std::vector<T>* (*lookup)(const char*)) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        const std::vector<T>* values = lookup(name.c_str());

        if (!values) {
            lua_pushnil(L);
            return 1;
        }

        lua_createtable(L, 0, static_cast<int>(values->size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < values->size(); ++i) {
            CoA::Script::PushNum(L, i + 1);

            if constexpr (std::is_same_v<T, float>) {
                CoA::Script::PushNum(L, (*values)[i]);
            } else {
                CoA::Script::PushInt(L, (*values)[i]);
            }

            lua_settable(L, -3);
        }

        return 1;
    }

    int GetIntVectorConfig(lua_State* L) {
        return GetVector<int32_t>(L, CoA::Config::IntVector);
    }

    int GetFloatVectorConfig(lua_State* L) {
        return GetVector<float>(L, CoA::Config::FloatVector);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Config", "GetIntConfig", GetIntConfig),
        COA_NATIVE("C_Config", "GetBoolConfig", GetBoolConfig),
        COA_NATIVE("C_Config", "GetFloatConfig", GetFloatConfig),
        COA_NATIVE("C_Config", "GetRateConfig", GetRateConfig),
        COA_NATIVE("C_Config", "GetIntVectorConfig", GetIntVectorConfig),
        COA_NATIVE("C_Config", "GetFloatVectorConfig", GetFloatVectorConfig),
    };
}

// FUN_10196c90.
int32_t CoA::Config::OnUpdateConfigs(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    ReadScalars<int32_t>(msg, s_store.ints, [](const std::string& n, int32_t o, int32_t v) {
        CoA::Events::Signal("INT_CONFIG_UPDATED", "%s%u%u", n.c_str(), o, v);
    });
    ReadScalars<bool>(msg, s_store.bools, [](const std::string& n, bool o, bool v) {
        CoA::Events::Signal("BOOL_CONFIG_UPDATED", "%s%b%b", n.c_str(), static_cast<int>(o), static_cast<int>(v));
    });
    ReadScalars<float>(msg, s_store.floats, [](const std::string& n, float o, float v) {
        CoA::Events::Signal("FLOAT_CONFIG_UPDATED", "%s%f%f", n.c_str(), static_cast<double>(o), static_cast<double>(v));
    });
    ReadScalars<float>(msg, s_store.rates, [](const std::string& n, float o, float v) {
        CoA::Events::Signal("RATE_CONFIG_UPDATED", "%s%f%f", n.c_str(), static_cast<double>(o), static_cast<double>(v));
    });

    if (CoA::Read::Remaining(msg) > 7) {
        ReadVectors<int32_t>(msg, s_store.intVectors);
        ReadVectors<float>(msg, s_store.floatVectors);
    }

    return 1;
}

const int32_t* CoA::Config::Int(const char* name) {
    return Find(s_store.ints, name);
}

const bool* CoA::Config::Bool(const char* name) {
    return Find(s_store.bools, name);
}

const float* CoA::Config::Float(const char* name) {
    return Find(s_store.floats, name);
}

const float* CoA::Config::Rate(const char* name) {
    return Find(s_store.rates, name);
}

const std::vector<int32_t>* CoA::Config::IntVector(const char* name) {
    return Find(s_store.intVectors, name);
}

const std::vector<float>* CoA::Config::FloatVector(const char* name) {
    return Find(s_store.floatVectors, name);
}

void CoA::Config::Reset() {
    s_store = Store();
}

void CoA::Config::Init() {
    COA_PACKET(0x58D, CoA::Config::OnUpdateConfigs);
    COA_EVENT("INT_CONFIG_UPDATED");
    COA_EVENT("BOOL_CONFIG_UPDATED");
    COA_EVENT("FLOAT_CONFIG_UPDATED");
    COA_EVENT("RATE_CONFIG_UPDATED");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
