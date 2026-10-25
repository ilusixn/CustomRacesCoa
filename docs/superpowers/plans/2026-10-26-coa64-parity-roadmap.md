# CoA64 inventory parity roadmap (Plans 9..15)

Goal (user, 2026-10-26): every row of `CoA/inventory-report.md` at total / total. Baseline after Plan 8: natives
725/1346, packets 342/533, hooks 126/346, byte patches 125/126, CVars 22/93, events 108/259.

Events: all 303 names are already registered in bulk (`Events.generated.inc`). The inventory counts an event as
ported only when a module emits it, so the event gap closes as the emitting modules land.

Each plan is its own branch, review and local merge (same process as Plans 4..8). The report is regenerated after
each merge. Modules are grouped by what a player meets first (the remaining world-entry errors lead), then by subsystem.
Line counts are the reconstruction's (pin 1b9ebca).

| Plan | Modules | Recon lines | Gap closed |
|---|---|---:|---|
| 9  | DraftMode, BuildDraft, CanToggle, HighRisk, Seasonal, ScrollOfFortune, Token | ~2.1k | `HasHandOfFateSacrificePick`, the mode toggles |
| 10 | Tutorial, Challenge, TrialCreator | ~4.5k | `C_Tutorial`, `C_Challenge` |
| 11 | BuildCreator, WildcardRolls, WildcardSets, WildcardRapid, TemporalContracts | ~5.6k | `C_BuildCreator`, the CA / filter Build Creator Deps |
| 12 | GMTicket, BugTracker, PlayerPoll, RecoveryService, CustomStore, Callboard(Cache), FriendsMetadata, ChatCheck, Discord | ~5k | `C_PlayerTicket`, `C_CustomStore` |
| 13 | Manastorm, SkillCardCollection, SkillCardSlots, LootLockout, ItemCollections, CollectorCache, ItemStats, POI, SpellCharges, Aura137, QuestAddon | ~4.6k | `C_Manastorm`, `C_SkillCardCollection`, `C_LootLockout` |
| 14 | ClientOptions, Camera, Graphics, NamePlates, UnitSelect, TooltipScaling, FontRender, ZoneLight, SpellVisuals, SpellShadow, LoadingScreen, Cursor, SceneTransform, TerrainScale, SpellMods, MovementForce | ~6.6k | the missing CVars (`useUiScale`, `autoAcceptTrades`, ...), most client hooks |
| 15 | InitHooks, CrashContext, AddonTaint, SmallHooks, ClientGuards, LargeAlloc, GossipQuest, Geosets, SpellTokens, PacketApi, Templates, AssetQuery, ModelNames, the single leftovers (AttachFixes / AttachPatches / AttachLua / GlobalsC / GlobalsJ / CharacterCreate / RealmData / MemoryBridge / Main / ClientDbcPatch / CAMgr / MysticEnchant) and hooks 0x63200D / 0x632A40 | ~3.5k | the rest, ending at total / total |

After Plan 15: DXVK, then Warden. No Helios.
