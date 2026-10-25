import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import inventory  # noqa: E402

RECON = os.environ.get('COA_RECON', 'F:/WorkDir/ascension-extensions-reconstruction')
REFERENCE = os.path.join(RECON, 'docs', 'DLL_REFERENCE.md')

FIXTURE = """## Lua API index

### (global)

| Name | State | Args | Returns | Original | Module |
|---|---|---|---|---|---|
| `AbandonSkillID` | world | () | 0 | `0x10A3CF90` | [AscGlobalsH](#ascglobalsh) |

### C_Realm

| Name | State | Args | Returns | Original | Module |
|---|---|---|---|---|---|
| `IsLive` | both | () | 1 | `0x100DBEC0` | [AscAccount](#ascaccount) |

## Server packet index (SMSG)

| Opcode | Name | Module |
|---|---|---|
| `0x563` | SMSG_LOYALTY_PASS_UPDATE | [AscSmallApis](#ascsmallapis) |

## Event index

| Event | Fired by |
|---|---|
| `ACTIVE_MANASTORM_UPDATED` | [AscManastorm](#ascmanastorm) |

## Modules

### AscAddonTaint

**Client hooks**

| Address | Kind | Line | Notes |
|---|---|---|---|
| `0x488540` | ReplaceFunction | 203 |  |

| CVar | Default | Flags | Category | List | Slot |
|---|---|---|---|---|---|
| `showToolsUI` | `0` | 0x1 | 5 | glue | `0x10BE0000` |
"""


class ParseReferenceTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile('w', suffix='.md', delete=False, encoding='utf-8')
        self.tmp.write(FIXTURE)
        self.tmp.close()

    def tearDown(self):
        os.unlink(self.tmp.name)

    def test_fixture_natives(self):
        inv = inventory.parse_reference(self.tmp.name)
        self.assertEqual(inv.natives, {('', 'AbandonSkillID', 'world'), ('C_Realm', 'IsLive', 'both')})

    def test_fixture_other_categories(self):
        inv = inventory.parse_reference(self.tmp.name)
        self.assertEqual(inv.packets, {0x563})
        self.assertEqual(inv.events, {'ACTIVE_MANASTORM_UPDATED'})
        self.assertEqual(inv.hooks, {0x488540})
        self.assertEqual(inv.cvars, {'showToolsUI'})


@unittest.skipUnless(os.path.exists(REFERENCE), 'reconstruction checkout not found')
class RealReferenceTest(unittest.TestCase):
    """Totals of the generated reference at the pinned commit 1b9ebca. The reconstruction README's own figures
    (1,277 / 530 / 400) count registrar entries only and fold byte patches into hooks; the doc lists everything
    that has to be ported: 70 widget/metatable methods with no registrar entry, 533 SMSG handlers, 346 hook sites,
    and the 126 byte patches come from the AscAttach*.generated.inc tables."""

    @classmethod
    def setUpClass(cls):
        cls.inv = inventory.parse_reference(REFERENCE)

    def test_native_pairs(self):
        self.assertEqual(len({(ns, name) for ns, name, _ in self.inv.natives}), 1346)

    def test_packets(self):
        self.assertEqual(len(self.inv.packets), 533)

    def test_hooks(self):
        self.assertEqual(len(self.inv.hooks), 346)

    def test_patches(self):
        self.assertEqual(len(inventory.parse_patches(RECON)), 126)

    def test_cvars(self):
        self.assertEqual(len(self.inv.cvars), 93)


class ScanCoaTest(unittest.TestCase):
    def test_scan_coa_reads_registration_macros(self):
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, 'CoA', 'Source'))
            with open(os.path.join(root, 'CoA', 'Source', 'A.cpp'), 'w', encoding='utf-8') as f:
                f.write('    COA_NATIVE("C_Chat", "Foo", Foo),\n'
                        '    COA_NATIVE("", "AbandonSkillID", AbandonSkillID),\n'
                        'COA_PACKET(0x563, OnLoyalty);\n'
                        'COA_CVAR("showToolsUI", "", 0x1, "0", &s_showTools, false);\n'
                        'COA_EVENT("ACTIVE_MANASTORM_UPDATED");\n'
                        '// CoA: AscAddonTaint.cpp:Hook (orig hook 0x488540)\n'
                        '// CoA: Extensions.dll patch 0x4DA8EB (AscAttachPatches) -- removed\n')
            inv = inventory.scan_coa(root)
            self.assertEqual({(ns, name) for ns, name, _ in inv.natives}, {('C_Chat', 'Foo'), ('', 'AbandonSkillID')})
            self.assertEqual(inv.packets, {0x563})
            self.assertEqual(inv.cvars, {'showToolsUI'})
            self.assertEqual(inv.events, {'ACTIVE_MANASTORM_UPDATED'})
            self.assertEqual(inv.hooks, {0x488540})
            self.assertEqual(inv.patches, {0x4DA8EB})

    def test_scan_coa_counts_dbc_patch_registrations(self):
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, 'CoA', 'Source', 'Runtime'))
            with open(os.path.join(root, 'CoA', 'Source', 'A.cpp'), 'w', encoding='utf-8') as f:
                f.write('    CoA::DbcPatch::Register(0x676, "DBFilesClient\\\\GlobalStrings.dbc", 0x10, 0, nullptr);\n'
                        '    CoA::DbcPatch::RegisterSized(0x579, "DBFilesClient\\\\LFGActivities.dbc", 0x1C, 1, nullptr);\n'
                        '    CoA::DbcPatch::RegisterInsertHook(\n        0x692, "DBFilesClient/Appearances.dbc", 0x44, 4, f);\n')
            with open(os.path.join(root, 'CoA', 'Source', 'Runtime', 'DbcPatch.generated.inc'), 'w', encoding='utf-8') as f:
                f.write('{0x56C, "DBFilesClient\\\\MythicAffixes.dbc", 0x40, 0x0ull},   // SMSG_PATCH_MYTHIC_AFFIXES\n')
            inv = inventory.scan_coa(root)
            self.assertEqual(inv.packets, {0x676, 0x579, 0x692, 0x56C})


if __name__ == '__main__':
    unittest.main()


class GlueStateCoverageTest(unittest.TestCase):
    def test_glue_state_coverage(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'BindingStates.generated.inc')
            with open(path, 'w', encoding='utf-8') as f:
                f.write('static const StateName kGlueBindings[] = {\n    {"", "A"},\n    {"C_X", "B"},\n};\n'
                        'static const StateName kWorldBindings[] = {\n    {"", "W"},\n};\n')
            glue = inventory.parse_glue_bindings(path)
        self.assertEqual(glue, {('', 'A'), ('C_X', 'B')})
        have = inventory.Inventory()
        have.natives.add(('', 'A', ''))
        text = inventory.report(inventory.Inventory(), have, glue)
        self.assertIn('natives (glue state): 1/2', text)
        self.assertIn('`C_X.B`', text)
