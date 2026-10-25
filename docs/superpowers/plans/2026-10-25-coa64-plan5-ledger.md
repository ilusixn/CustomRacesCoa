# SDD ledger — plan: docs/superpowers/plans/2026-10-25-coa64-plan5-world-entry-errors-2.md
Ruling: plan order changed from the Plan 4 findings (CA next) to the measured error counts (widget methods + small modules, then MysticEnchant as Plan 6, CA as Plan 7) -- cost if wrong: none, all are done in turn.
Ruling: -extlog now logs every Lua error text through the error-handler hook (CoA64 diagnostic, not in the original, off by default).
Task 1: Ruling: MovieFrame:SetVolume calls the frame's own m_sound (+0x29C).SetVolume -- the original passes the frame as `this`, so SESound::m_internal is read from the frame's vtable pointer and the volume write lands in client memory; reproducing that is a memory corruption, not a behavior -- cost if wrong: a movie's volume follows SetVolume where the original's did not.
Task 1: Ruling: the 0x59714D byte is modelled as a tri-state (0x74 stock je, 0x75 jne, 0xEB jmp); SetSpell(0) writes 0x75 there, which inverts RotateModel's HasSequence test (the original's bug, kept) -- cost if wrong: model frames turn when they lack the turn sequence after SetSpell(0), as in the original.
Task 1: Ruling: SetPOIMaxRange's 0xA41E00 float becomes g_maxPOIDistance, read only by the edge-POI distance test (the only reader, 0x7F4664; the distances[] seed is an immediate) -- cost if wrong: none found.
Task 1: Ruling: SetPOIMaxRange lives in WidgetClient until AscPOI's packets/natives are ported (then it moves) -- cost if wrong: one move.
Task 1: Ruling: SetUnitGUID and GetAllAttributes keep the original's missing self type checks (any Model frame reaches CGCharacterModelBase::RefreshUnit) -- cost if wrong: a script calling them on the wrong widget corrupts memory, as in the original.
Task 1: complete (commits e174d51..ce07462, tests: cmd //c Tools\coa\check.bat → OK)
Task 2: Ruling: TargetClosestEnemy keeps the original's taint handling (lua_tainted cleared for the scan, restored only when a candidate is found) -- cost if wrong: a call with no attackable unit leaves the caller untainted, as in the original; add-ons already run secure (Plan 4 signature ruling).
Task 2: Ruling: ExtraActionButton's timer re-arms without storing its id, so Stop cancels only the first (stale) id and each world entry adds a chain (test ExtraActionButton_EachWorldEntryAddsAChain), as SpellActivationOverlay -- cost if wrong: one more 100 ms tick per relog.
Task 2: Ruling: the reconstruction's ExtraActionButton "first tick" log line is a reconstruction diagnostic, not ported -- cost if wrong: one missing log line.
Task 2: Ruling: InteractNearest / TargetClosestEnemy dereference the active player unchecked, as the original (world-only natives) -- cost if wrong: a crash if called without a player.
Task 2: complete (commits ce07462..2327edf, tests: cmd //c Tools\coa\check.bat → OK)
Task 3: complete (commits 2327edf..aa2ed0c, tests: cmd //c Tools\coa\check.bat → OK)
Task 1: Ruling: OnPrecastEnd passes the cast kit's m_animID as OnCastEnd's argument, which OnCastEnd looks up as a SpellVisual id (the original's bug, kept) -- cost if wrong: after a cast-time spell the model usually does not reset to stand, as in the original.
Task 3: Ruling: CombatFeedback.lua:107 did not reproduce after Task 1/2, so no separate fix; most likely an aborted OnLoad (missing widget method) skipped CombatFeedback_Initialize -- cost if wrong: it returns once another OnLoad error appears; watched in later captures.
Final review: opus subagent -- 0 Critical, 0 Important, 5 Minor.
Final: minor (deferred): -extlog's lua_tostring(L,-1) converts a numeric error value to a string in place (only with -extlog, only error(<number>)).
Final: minor (deferred): ExtraActionButton_EachWorldEntryAddsAChain never fires OnTimer, so the stale-id re-arm is not exercised.
Final: minor (deferred): no tests for the RotateModel/UpdateModel jump tri-state or GetAllAttributes on an empty list (client code, plan scopes tests to pure parts).
