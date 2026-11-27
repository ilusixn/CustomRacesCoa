"""Inventory of the client functions that assume a 25-slot quest log.

Run from the repo root: python -I Tools/quest_log_sites.py > docs/quest-log-64-sites.md
"""
import glob
import re
import sys

KINDS = ("slot-loop", "slot-index", "list-size", "level-xp", "ignore")

SORT_NOTE = "collapse-filter bit test, keeps < 25 guard (spec non-goal); m_sortTypes storage itself is the list-size row"
DESC_NOTE = "descriptor range; stays 25 (anti-goal)"
RELOCATED = "reads CGQuestLog::m_quests, which holds all 64 slots (128 rows); the DLL reads AscQuestLog64::QuestListRows"

# Rows kept with a kind and a note instead of the scanned ones: (kind, note).
FORCE = {
    "QuestInLog": ("list-size", RELOCATED),
    "NeedsKill": ("list-size", RELOCATED + "; via ForOpenLoggedQuest"),
    "NeedsItem": ("list-size", RELOCATED + "; via ForOpenLoggedQuest"),
}
# Notes added to the scanned rows of a function.
NOTES = {
    "OnUpdateQuest": "slot comes from a descriptor offset, so it stays 0-24; the refresh path for slots 25-63 is separate",
}

# Per function: the reason a hit is not a quest-log site. Matched on the function's name.
IGNORE = {
    "QSortQuestLogEntries": SORT_NOTE,
    "CGQuestLog::FilterAndSortQuests": SORT_NOTE,
    "SetActiveMirrorHandlers": DESC_NOTE,
    "UnsetActiveMirrorHandlers": DESC_NOTE,
    "ScriptEventsRegisterUnit": DESC_NOTE,
    "ReportDailyQuestsRemaining": "iterates m_playerFieldDailyQuests1, the daily-quest list, not log slots",
    "Script_GetDailyQuestsCompleted": "counts m_playerFieldDailyQuests1, the daily-quest list, not log slots",
    "QuestFailedCallback": "25 is the daily-quest limit in an error string; case 50 is a failure reason",
    "CGPlayer_C::OnQuestGiverQuestFailed": "25 is the daily-quest limit in an error string; case 50 is a failure reason",
    "ScriptEventsUnregisterUnit": "unsets the quest-log handler by start offset 0x28; no slot bound",
    "CoA::InitHooksClient::Init": "wires the questLogQuest callback; the slot read is in QuestLogQuest",
}

LOG = re.compile(r"m_playerQuestLog|GetQuestLogEntry|kQuestLogSlots|PlayerQuestLogEntry|questLogQuest")
LIST = re.compile(r"m_questPOIs|m_questWatches|m_numQuestWatches|m_quests\b")
BOUND = r"(?:25|0x19u?|24|0x18|50|0x32)"
# The detours the InitHooks.cpp decisions serve, from the "orig hook" comments at their WoW call sites.
DLL_HOOK = {
    "CoA::InitHooks::QuestExperience": "0x005E0420",
    "CoA::InitHooks::QuestLevel": "0x005E0D20",
    "CoA::InitHooks::QuestLinkLevel": "0x0061E5C0",
}
HOOK = re.compile(r"orig hook (0x[0-9A-Fa-f]+)|(FUN_[0-9a-f]+|sub_[0-9a-f]+)")
SKIP_LINE = re.compile(r"m_sortTypes|sortId|sortA|sortB|DailyQuests|DisplayError|inventoryType|case \d+:")


def strip(line):
    line = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line)
    line = re.sub(r"/\*.*?\*/", "", line)
    return re.sub(r"//.*", "", line)


def functions(path):
    """Yield (name, offset, comment_text, first_line_no, [(line_no, code)])."""
    lines = open(path, encoding="utf-8", errors="ignore").read().split("\n")
    depth, ns_depth, i = 0, 0, 0
    while i < len(lines):
        code = strip(lines[i])
        if depth == ns_depth and re.match(r"\s*namespace\b.*\{\s*$", code):
            depth += 1
            ns_depth += 1
            i += 1
            continue
        if ns_depth and depth == ns_depth and re.match(r"\s*\}", code):
            depth -= 1
            ns_depth -= 1
            i += 1
            continue
        head = re.match(r"(?!\s*(?:if|for|while|switch|else|do|return|catch)\b)\s*[\w:<>~*&, ]+\(", code)
        if depth == ns_depth and head and not code.rstrip().endswith(";"):
            j = i
            while j < len(lines) and not strip(lines[j]).rstrip().endswith("{"):
                if strip(lines[j]).rstrip().endswith(";"):
                    j = -1
                    break
                j += 1
            if j >= 0 and j < len(lines):
                end, level = j, 0
                while end < len(lines):
                    level += strip(lines[end]).count("{") - strip(lines[end]).count("}")
                    if level == 0:
                        break
                    end += 1
                back, comment = i - 1, []
                while back >= 0 and (lines[back].strip().startswith("//") or not lines[back].strip()):
                    comment.append(lines[back])
                    back -= 1
                    if len(comment) > 12:
                        break
                text = "\n".join(reversed(comment))
                off = re.search(r"// OFFSET: (0x[0-9A-Fa-f]+)", text)
                sig = " ".join(strip(lines[k]).strip() for k in range(i, j + 1))
                named = re.search(r"([\w:~]+)\s*\(", sig)

                if not named:
                    i += 1
                    continue

                name = named.group(1)
                yield name, off.group(1) if off else None, text, i + 1, [(k + 1, strip(lines[k])) for k in range(i, end + 1)]
                i = end + 1
                continue
        depth += strip(lines[i]).count("{") - strip(lines[i]).count("}")
        i += 1


def classify(body):
    """Return {kind: first line number} for one function body."""
    text = "\n".join(c for _, c in body)
    found = {}

    def add(kind, line):
        found.setdefault(kind, line)

    for idx, (no, code) in enumerate(body):
        if SKIP_LINE.search(code) and not LOG.search(code):
            continue
        near = "\n".join(c for _, c in body[idx:idx + 9])
        for_bound = re.search(r"\bfor\s*\([^;]*;[^;]*?[<]=?\s*" + BOUND + r"\b", code)
        decl = re.search(r"\b\w+\s+\w+\s*\[(?:25|50)\]", code)
        if "kQuestLogSlots" in code and "const" in code:
            add("slot-loop", no)
        elif decl and (LIST.search(text) or LOG.search(text)):
            add("list-size", no)
        elif for_bound:
            if re.search(r"m_sortTypes", near):
                continue
            if LOG.search(near):
                add("slot-loop", no)
            elif LIST.search(near):
                add("list-size", no)
        elif re.search(r"\bfor\s*\([^;]*;[^;]*<\s*kQuestLogSlots", code):
            add("slot-loop", no)
        elif re.search(r"i < 0x21C", code):
            add("slot-loop", no)
        elif "PlayerQuestLogChangedHandler" in code and "0x1F4" in code:
            add("slot-loop", no)
        elif re.search(r"\bslot\b[^;?]*[<>]=?\s*(?:25|24)\b", code) or re.search(r"\(\s*offset - 0x28\s*\)", code):
            add("slot-index", no)
        elif LIST.search(text) and re.search(r"(?:\bindex\b|\bwatch\b|\bi\b|\bm_numQuestWatches\b|\bi\b)[^;]*[<>]=?\s*(?:25|24|50)\b", code):
            add("list-size", no)
        if re.search(r"InitHooks::Quest(?:Level|Experience|LinkLevel)\(", code) or re.search(r"\+ 0x(?:24|3D)\)", code):
            add("level-xp", no)
        if re.search(r"\(\s*static_cast<uint32_t>\(slot\)\s*\+\s*0x(?:24|3D)\s*\)|i\s*\+\s*0x3D", code):
            add("level-xp", no)
    if LOG.search(text) and not {"slot-loop", "slot-index"} & set(found):
        line = next(no for no, c in body if LOG.search(c))
        found["slot-index"] = line
    return found


def hook_of(text, body_text):
    m = HOOK.search(text + "\n" + body_text[:1200])
    if not m:
        return "—"
    return m.group(1) or "dll:" + m.group(2)


def scan():
    rows = []
    paths = sorted(glob.glob("WoW/Source/**/*.cpp", recursive=True) + glob.glob("CoA/Source/Modules/*.cpp"))
    for path in paths:
        if "CGQuestInfo" in path or "GossipInfo" in path:
            continue
        path = path.replace("\\", "/")
        coa = path.startswith("CoA")
        for name, off, comment, line, body in functions(path):
            joined = "\n".join(c for _, c in body)
            if not (LOG.search(joined) or LIST.search(joined) or "Quest" in name or re.search(r"InitHooks::Quest|OnUpdateQuest|PlayerQuestLogChangedHandler", joined)):
                continue
            if name.startswith(("CGQuestInfo::", "CGGossipInfo::")):
                continue
            found = classify(body)
            offset = hook_of(comment, "\n".join(l for l in open(path, encoding="utf-8", errors="ignore").read().split("\n")[line - 1:line + 8])) if coa else (off or "—")
            offset = DLL_HOOK.get(name, offset)
            short = name.split("::")[-1] if "::" in name else name
            ignored = IGNORE.get(name) or IGNORE.get(short)
            if ignored:
                rows.append((offset, name, path, line, "ignore", ignored))
                continue
            if short in FORCE:
                rows.append((offset, name, path, line, FORCE[short][0], FORCE[short][1]))
                continue
            for kind, no in sorted(found.items(), key=lambda kv: KINDS.index(kv[0])):
                rows.append((offset, name, path, no, kind, NOTES.get(short, "")))
            for no, code in body:
                if re.search(r"m_sortTypes\[\w+\]\s*=[^=]", code):
                    rows.append((offset, name, path, no, "list-size", "appends a sort type with no bound check; m_sortTypes storage grows to 64 (spec)"))
                elif re.search(r"\bsortId\s*>=\s*25", code):
                    rows.append((offset, name, path, no, "ignore", SORT_NOTE))
    return rows


def data_rows():
    """The arrays whose declaration fixes the list sizes."""
    pat = re.compile(r"(m_quests|m_questWatches|m_questPOIs|m_playerQuestLog|m_sortTypes)\s*\[(\d+)\]")
    rows = []
    for path in sorted(glob.glob("WoW/Source/**/*.h", recursive=True) + glob.glob("WoW/Source/Ui/QuestLog.cpp")):
        path = path.replace("\\", "/")
        if "Gossip" in path:
            continue
        for no, raw in enumerate(open(path, encoding="utf-8", errors="ignore"), 1):
            code = strip(raw).strip()
            m = pat.search(code)
            if m and code.endswith(";") and re.match(r"^(?:static )?\w+ (?:CGQuestLog::)?m_\w+\s*\[", code):
                kind, note = "list-size", ""

                if m.group(1) == "m_playerQuestLog":
                    kind, note = "ignore", DESC_NOTE
                elif m.group(1) == "m_sortTypes":
                    note = "storage grows to 64 (spec); the collapse-filter bit tests keep their < 25 guard"

                rows.append(("—", "(data) " + m.group(1) + "[" + m.group(2) + "]", path, no, kind, note))
    for path in sorted(glob.glob("CoA/Source/Modules/*.cpp")):
        for no, raw in enumerate(open(path, encoding="utf-8", errors="ignore"), 1):
            if re.search(r"\bkQuestLogSlots\s*=", strip(raw)):
                rows.append(("—", "(data) kQuestLogSlots", path.replace("\\", "/"), no, "slot-loop", ""))
    return rows


def main():
    rows = data_rows() + scan()
    out = ["# Quest-log sites that assume 25 slots", "",
           "Generated by `python -I Tools/quest_log_sites.py`. Do not edit by hand.", "",
           "| OFFSET | function | file:line | kind | note |", "|---|---|---|---|---|"]
    for off, name, path, line, kind, note in rows:
        out.append("| %s | `%s` | %s:%d | %s | %s |" % (off, name, path, line, kind, note))
    counts = {k: sum(1 for r in rows if r[4] == k) for k in KINDS}
    out += ["", "Rows: " + ", ".join("%s %d" % kv for kv in counts.items()) + "."]
    sys.stdout.buffer.write(("\n".join(out) + "\n").encode("utf-8"))


main()
