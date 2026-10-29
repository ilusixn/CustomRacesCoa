// Client-side persistence of the body sliders (the sliders themselves are the Esteria client's BodyShape system, ported into
// Engine/Source/Retail/BodyShape.h). Values are kept per character name in WTF\EsteriaBody.txt: the character that is created right
// after the creator's Body page adopts what the creator showed; every other login restores its own. Nothing is sent to the server.
#include "CoA/Source/Modules/Esteria/EsteriaCompat.h"
#include "CoA/Source/Modules/EsteriaRaces.h"
#include "Engine/Source/Retail/BodyShape.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <sstream>
#include <string>

namespace {
    using Slots = std::map<std::string, float>;

    const char* const kFile = "WTF\EsteriaBody.txt";

    std::map<std::string, Slots> s_saved;
    Slots s_pending;
    std::time_t s_pendingTime = 0;
    bool s_loaded = false;
    std::string s_loadedFor;
    std::time_t s_lastCheck = 0;

    std::string Pack(Slots const& slots) {
        std::ostringstream out;

        for (auto const& entry : slots) {
            out << ' ' << entry.first << '=' << entry.second;
        }

        return out.str();
    }

    Slots Unpack(std::string const& text) {
        Slots slots;
        std::istringstream in(text);
        std::string token;

        while (in >> token) {
            size_t eq = token.find('=');

            if (eq != std::string::npos && eq > 0) {
                slots[token.substr(0, eq)] = static_cast<float>(std::atof(token.c_str() + eq + 1));
            }
        }

        return slots;
    }

    void Save() {
        FILE* file = fopen(kFile, "w");

        if (!file) {
            return;
        }

        if (!s_pending.empty() && s_pendingTime) {
            fprintf(file, "*pending %lld%s\n", static_cast<long long>(s_pendingTime), Pack(s_pending).c_str());
        }

        for (auto const& entry : s_saved) {
            fprintf(file, "%s 0%s\n", entry.first.c_str(), Pack(entry.second).c_str());
        }

        fclose(file);
    }

    void Load() {
        if (s_loaded) {
            return;
        }

        s_loaded = true;
        FILE* file = fopen(kFile, "r");

        if (!file) {
            return;
        }

        char line[2048];

        while (fgets(line, sizeof(line), file)) {
            std::istringstream in(line);
            std::string name;
            long long when = 0;
            in >> name >> when;
            std::string rest;
            std::getline(in, rest);
            Slots slots = Unpack(rest);

            if (name == "*pending") {
                s_pending = slots;
                s_pendingTime = static_cast<std::time_t>(when);
            } else if (!name.empty()) {
                s_saved[name] = slots;
            }
        }

        fclose(file);
    }

    std::string KeyOf(const char* name) {
        std::string key = name ? name : "";

        for (char& c : key) {
            if (c == ' ') {
                c = '_';
            }
        }

        return key;
    }

    Slots NonZero() {
        Slots out;

        for (auto const& entry : BodyShape::Local().v) {
            if (entry.second != 0.0f) {
                out[entry.first] = entry.second;
            }
        }

        return out;
    }
}

namespace CoA::Esteria::BodySliders {
    // The creator's Finish button: remember what the Body page showed for the character about to be created.
    void SavePending() {
        Load();
        s_pending = NonZero();
        s_pendingTime = s_pending.empty() ? 0 : std::time(nullptr);
        Save();
    }

    // Called every frame for the local player in the world, before the engine applies BodyShape::Local() to the model.
    void SyncPlayer() {
        const char* name = ClientServices::GetCharacterName();

        if (!name || !*name) {
            return;
        }

        Load();
        std::string key = KeyOf(name);

        if (key != s_loadedFor) {
            s_loadedFor = key;
            auto found = s_saved.find(key);

            if (found != s_saved.end()) {
                BodyShape::Local().v = found->second;
            } else if (!s_pending.empty() && s_pendingTime && std::time(nullptr) - s_pendingTime < 6 * 3600) {
                BodyShape::Local().v = s_pending;      // a character made right after the sliders were set
                s_saved[key] = s_pending;
                s_pending.clear();
                s_pendingTime = 0;
                Save();
            } else {
                BodyShape::Local().v.clear();         // an existing character without saved sliders
            }
        }

        std::time_t now = std::time(nullptr);

        if (now != s_lastCheck) {
            s_lastCheck = now;
            Slots current = NonZero();
            Slots& stored = s_saved[key];

            if (current != stored) {
                if (current.empty()) {
                    s_saved.erase(key);
                } else {
                    stored = current;
                }

                Save();
            }
        }
    }

    // The in-world Body panel's Save button: write the current sliders for this character right away (they are also kept once a second).
    void SaveNow() {
        const char* name = ClientServices::GetCharacterName();
        if (!name || !*name) {
            return;
        }
        Load();
        Slots current = NonZero();
        std::string key = KeyOf(name);
        if (current.empty()) {
            s_saved.erase(key);
        } else {
            s_saved[key] = current;
        }
        Save();
    }

    void Init() {}
}
