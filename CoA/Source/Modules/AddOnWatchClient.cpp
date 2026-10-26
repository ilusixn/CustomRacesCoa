#include "CoA/Source/Modules/AddOnWatch.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmDataClient.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Ui/AddOns.h"
#include <string>
#include <vector>
#include <windows.h>

// The live addon list's client half (AddOnWatch.h): an overlapped ReadDirectoryChangesW on each addon folder, checked
// once per frame without a syscall (HasOverlappedIoCompleted); relevant changes are settled by AddOnWatch, then
// RescanAddOns re-reads only those addons and ADDON_LIST_UPDATE tells the UI (the stock glue event; queued for the
// world table).
namespace {
    constexpr uint32_t kRetryMs = 5000;
    constexpr DWORD kFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;

    struct Watch {
        std::string path;
        HANDLE dir = INVALID_HANDLE_VALUE;
        OVERLAPPED overlapped = {};
        alignas(DWORD) uint8_t buffer[16384];
        uint32_t nextOpen = 0;
        // The first open attempt (InitializeAddOns scans at login anyway); any later open is the folder appearing.
        bool first = true;
    };

    std::vector<Watch*> s_watches;

    void Close(Watch& watch) {
        if (watch.dir != INVALID_HANDLE_VALUE) {
            CancelIo(watch.dir);
            CloseHandle(watch.dir);
            watch.dir = INVALID_HANDLE_VALUE;
        }

        if (watch.overlapped.hEvent) {
            CloseHandle(watch.overlapped.hEvent);
            watch.overlapped.hEvent = nullptr;
        }
    }

    bool Issue(Watch& watch) {
        HANDLE event = watch.overlapped.hEvent;
        watch.overlapped = {};
        watch.overlapped.hEvent = event;

        return ReadDirectoryChangesW(watch.dir, watch.buffer, sizeof(watch.buffer), TRUE, kFilter, nullptr, &watch.overlapped, nullptr) != 0;
    }

    bool Open(Watch& watch) {
        watch.dir = CreateFileA(watch.path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

        if (watch.dir == INVALID_HANDLE_VALUE) {
            return false;
        }

        watch.overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);

        if (!watch.overlapped.hEvent || !Issue(watch)) {
            Close(watch);
            return false;
        }

        CoA::Log::Printf("AddOnWatch: watching %s", watch.path.c_str());
        return true;
    }

    void Collect(const Watch& watch, DWORD bytes, uint32_t now) {
        const uint8_t* cursor = watch.buffer;

        while (true) {
            const FILE_NOTIFY_INFORMATION* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);
            int32_t chars = static_cast<int32_t>(info->FileNameLength / sizeof(WCHAR));
            int32_t size = WideCharToMultiByte(CP_UTF8, 0, info->FileName, chars, nullptr, 0, nullptr, nullptr);

            if (size > 0) {
                std::string path(static_cast<size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, info->FileName, chars, path.data(), size, nullptr, nullptr);
                std::string name = CoA::AddOnWatch::AddOnForChange(path.c_str());

                if (!name.empty()) {
                    CoA::AddOnWatch::Note(name, now);
                }
            }

            if (!info->NextEntryOffset || cursor + info->NextEntryOffset >= watch.buffer + bytes) {
                break;
            }

            cursor += info->NextEntryOffset;
        }
    }

    void Poll(Watch& watch, uint32_t now) {
        if (watch.dir == INVALID_HANDLE_VALUE) {
            if (static_cast<int32_t>(now - watch.nextOpen) < 0) {
                return;
            }

            if (!Open(watch)) {
                watch.nextOpen = now + kRetryMs;
            } else if (!watch.first) {
                // The folder appeared or came back: whatever is in it now may be new.
                CoA::AddOnWatch::NoteAll(now);
            }

            watch.first = false;
            return;
        }

        if (!HasOverlappedIoCompleted(&watch.overlapped)) {
            return;
        }

        DWORD bytes = 0;

        if (!GetOverlappedResult(watch.dir, &watch.overlapped, &bytes, FALSE)) {
            // The folder went away (or the handle broke): drop it and rescan everything.
            Close(watch);
            watch.nextOpen = now + kRetryMs;
            CoA::AddOnWatch::NoteAll(now);
            return;
        }

        if (bytes == 0) {
            // The buffer overflowed: the changes are lost, so everything has to be rescanned.
            CoA::AddOnWatch::NoteAll(now);
        } else {
            Collect(watch, bytes, now);
        }

        if (!Issue(watch)) {
            Close(watch);
            watch.nextOpen = now + kRetryMs;
            CoA::AddOnWatch::NoteAll(now);
        }
    }

    void Tick() {
        uint32_t now = OsGetAsyncTimeMs();

        for (Watch* watch : s_watches) {
            Poll(*watch, now);
        }

        std::vector<std::string> names;
        bool all = false;

        if (!CoA::AddOnWatch::Take(now, names, all)) {
            return;
        }

        std::vector<const char*> list;

        for (const std::string& name : names) {
            list.push_back(name.c_str());
        }

        if (!RescanAddOns(list.data(), static_cast<uint32_t>(list.size()), all)) {
            return;
        }

        CoA::Log::Printf("AddOnWatch: rescanned %s (%u addons, %u in the list)", all ? "all" : "changed", static_cast<unsigned>(list.size()), GetNumAddOns());
        CoA::Events::Signal("ADDON_LIST_UPDATE", nullptr);
    }

    void AddWatch(const char* path) {
        Watch* watch = new Watch();
        watch->path = path;
        s_watches.push_back(watch);
    }
}

void CoA::AddOnWatchClient::Init() {
    // Stock in the glue table; the world table gets it as a custom so in-game addon managers can listen too.
    CoA::Events::Queue("ADDON_LIST_UPDATE");

    AddWatch("Interface\\AddOns");

    if (const char* dir = CoA::RealmDataClient::AddOnsPath()) {
        AddWatch(dir);
    }

    CoA::Lifecycle::OnAfterPaint(Tick);
}
