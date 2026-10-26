#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/FrameScript/FrameScript.h"

// AscTooltipInsertFix (not in the genuine DLL; the reconstruction applies it early). Ascension's Tooltip:InsertLine /
// Tooltip:InsertLines (patch-B, SharedXML\TypeExtensions\Tooltip.lua) write a new line into GameTooltipTextLeft<i>
// whenever that font string exists, even past NumLines(). Font strings survive from an earlier, longer tooltip, so the
// text lands on a hidden line and the AddLine calls that follow overwrite it: a keystone tooltip keeps one Dungeon
// Modifier of several. After every world entry (CGGameUI::Initialize enters the world again on a /reload) both methods
// on the shared GameTooltip method table are wrapped: for the length of the call the globals of the lines past
// NumLines() are hidden, so the original's own "else AddLine" branch runs for them; they are restored afterwards.
// Temporary: to be removed once patch-B's Tooltip.lua carries the fix (`if line and i <= numLines`).
namespace {
    const char* const kFix = R"lua(
local get = GetGameTooltipMetatable
local methods = get and get().__index
if not methods then return end
for _, name in ipairs({ "InsertLine", "InsertLines" }) do
    local original = methods[name]
    if original and not methods["AscFixed" .. name] then
        methods["AscFixed" .. name] = true
        methods[name] = function(self, ...)
            local prefix = self:GetName()
            if not prefix then
                return original(self, ...)
            end
            prefix = prefix .. "TextLeft"
            local hidden = {}
            local i = self:NumLines() + 1
            while _G[prefix .. i] do
                hidden[i] = _G[prefix .. i]
                _G[prefix .. i] = nil
                i = i + 1
            end
            local ok, err = pcall(original, self, ...)
            for index, line in pairs(hidden) do
                _G[prefix .. index] = line
            end
            if not ok then
                error(err, 0)
            end
        end
    end
end
)lua";

    void Apply() {
        FrameScript_Execute(kFix, "AscTooltipInsertFix", nullptr);
    }
}

void CoA::TooltipInsertFixClient::Init() {
    CoA::ClientHooks::afterEnterWorld.Add(Apply);
}
