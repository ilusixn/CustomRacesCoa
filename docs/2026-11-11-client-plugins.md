# Client plugins (2026-11-11)

CoA64 loads every `Plugins\*.dll` beside the exe once, and calls each one's exported
`CoaPlugin_OpenLua(const CoaPluginLua* api, lua_State* L)` every time the in-game Lua state is built (login and
`/reload`), right after the client's own script functions and before FrameXML and the add-ons load. `api` carries the
Lua calls a plugin may make by pointer (`WoW/Source/Ui/ClientPlugins.h`), so a plugin needs no Lua of its own and is
not tied to the exe's code or addresses.

Why: Living Voices (quest voice-over) and Companion Voice (chat read aloud by a local speech model) bring their audio
engine as a DLL that patches the 32-bit stock Wow.exe 12340 at fixed addresses. CoA64 has neither the addresses nor
the bitness, so it offers this loader instead, and the engines are built as 64-bit plugins that keep their Lua API
(`LivingVoices.*`, `CompanionVoiceEngine.*`), so their add-ons run unchanged.

The loader is CoA64's own code and holds nothing of the plugins; a plugin stays a separate program under its own
licence (Living Voices and Companion Voice are GPL-3.0). Without a `Plugins` folder nothing changes.
