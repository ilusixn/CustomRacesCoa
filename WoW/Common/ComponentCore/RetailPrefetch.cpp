// Not in the 3.3.5a binary (CoA). /retailPrefetch reads RetailPrefetch.txt (one retail\<id>.m2 / .blp per line) through the
// Retail source so every model's skins, the common animations and the textures are downloaded and decoded into
// Data\Retail\Cache once, in a background thread; Pack_Retail_Cache.bat then packs them into patch-Retail-001.MPQ.
#if defined(WOWDECOMP_RETAIL_ASSETS)
#include "Engine/Source/ConsoleBase/ConsoleCommand.h"
#include "Engine/Source/ConsoleBase/IConsole.h"
#include "Engine/Source/Retail/CascSource.h"
#include "Engine/Source/BlizzardCore/Source/System/Log/Log.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {
// shortcut: only the animations a mount plays (stand, walk, run, jump, fall, swim, fly, rider mount); other ids load on demand.
bool WantedAnimation(const std::string& dependency) {
    const auto dash = dependency.find_last_of('-');
    if (dash == std::string::npos || dash < 4) return false;
    const unsigned id = static_cast<unsigned>(std::atoi(dependency.c_str() + dash - 4));
    const unsigned variation = static_cast<unsigned>(std::atoi(dependency.c_str() + dash + 1));
    return variation == 0 && (id <= 50 || id == 91 || (id >= 157 && id <= 160));
}

void Run() {
    static const char* log = "Logs/RetailPrefetch.log";
    std::ifstream list("RetailPrefetch.txt");
    std::vector<std::string> names;
    for (std::string line; std::getline(list, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty() && line[0] != '#') names.push_back(line);
    }
    Blizzard::Log::Printf(1, log, "start: %zu entries\n", names.size());
    const auto start = std::chrono::steady_clock::now();
    size_t failed = 0, files = 0;
    for (size_t i = 0; i < names.size(); ++i) {
        auto& source = Retail::SourceForPath(names[i].c_str());
        std::vector<uint8_t> bytes;
        std::string error;
        if (!source.Read(names[i].c_str(), bytes, error)) {
            ++failed;
            Blizzard::Log::Printf(1, log, "%s: %s\n", names[i].c_str(), error.c_str());
            continue;
        }
        ++files;
        if (names[i].size() > 3 && names[i].compare(names[i].size() - 3, 3, ".m2") == 0) {
            for (const auto& dependency : source.Dependencies(names[i].c_str())) {
                if (dependency.size() < 5 || dependency.compare(dependency.size() - 5, 5, ".anim") != 0 || !WantedAnimation(dependency))
                    continue;
                if (source.Read(dependency.c_str(), bytes, error)) ++files;
            }
            source.ReleaseModel(names[i].c_str());
        }
        if ((i + 1) % 25 == 0 || i + 1 == names.size()) {
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            Blizzard::Log::Printf(1, log, "[%-30s] %zu/%zu  files %zu  failed %zu  eta %ds\n",
                std::string(30 * (i + 1) / names.size(), '#').c_str(), i + 1, names.size(), files, failed,
                static_cast<int>(elapsed / (i + 1) * (names.size() - i - 1)));
        }
    }
    Blizzard::Log::Printf(1, log, "done: %zu files, %zu failed\n", files, failed);
}

int32_t PrefetchCommand(const char*, const char*) {
    static std::atomic<bool> running{false};
    if (running.exchange(true)) {
        ConsoleWrite("retailPrefetch is already running (Logs/RetailPrefetch.log)", ERROR_COLOR);
        return 1;
    }
    std::thread([] { Run(); running = false; }).detach();
    ConsoleWrite("retailPrefetch started in the background: progress in Logs/RetailPrefetch.log", DEFAULT_COLOR);
    return 1;
}
}

void RetailPrefetchRegister() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    ConsoleCommandRegister("retailPrefetch", PrefetchCommand, CATEGORY_GAME,
        "Downloads and caches every retail model listed in RetailPrefetch.txt (progress in Logs/RetailPrefetch.log)");
}
#else
void RetailPrefetchRegister() {}
#endif
