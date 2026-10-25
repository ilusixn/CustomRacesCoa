import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import addrmap  # noqa: E402


def write(root, rel, text):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    return path


class IndexOffsetsTest(unittest.TestCase):
    def test_index_offsets_names_the_tagged_function(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, 'WoW/Source/Client.cpp', '// OFFSET: 0x00405DD0\nstatic void OpenArchives() {\n}\n')
            index = addrmap.index_offsets(root)
            self.assertIn(0x405DD0, index)
            symbol, path, line = index[0x405DD0]
            self.assertEqual(symbol, 'OpenArchives')
            self.assertTrue(path.endswith('Client.cpp'))
            self.assertEqual(line, 2)

    def test_index_offsets_skips_verify_and_comment_lines(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, 'a.cpp', '// OFFSET: 0x00401200\n// VERIFY: 204ba559 auto\nint32_t CompareStrings(const void* a) {\n')
            self.assertEqual(addrmap.index_offsets(root)[0x401200][0], 'CompareStrings')

    def test_index_offsets_qualified_member(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, 'a.cpp', '// OFFSET: 0x004E0F50\nint32_t CCharacterCreation::GetRandomClassID() {\n')
            self.assertEqual(addrmap.index_offsets(root)[0x4E0F50][0], 'CCharacterCreation::GetRandomClassID')


class ScanReconTest(unittest.TestCase):
    def test_scan_recon_finds_client_addresses_only(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, 'src/A.cpp',
                  'reinterpret_cast<int(__cdecl*)(int)>(0x81B530)(1);\n'
                  'Register(0x10A3CF90);\n'
                  'if (x == 0x7F) {}\n')
            refs = addrmap.scan_recon(root)
            self.assertEqual(set(refs), {0x81B530})
            self.assertEqual(refs[0x81B530], ['src/A.cpp:1'])


class MergeManualTest(unittest.TestCase):
    def test_merge_manual_keeps_manual_row(self):
        auto = {0xD4139C: addrmap.Row(0xD4139C, 'data', ['src/A.cpp:29'], '', '', 'unported')}
        manual = {0xD4139C: addrmap.Row(0xD4139C, 'data', [], 'g_runningAddOn', 'WoW/Source/Ui/GameUI.cpp:10', 'manual')}
        merged = addrmap.merge(auto, manual)
        self.assertEqual(merged[0xD4139C].status, 'manual')
        self.assertEqual(merged[0xD4139C].symbol, 'g_runningAddOn')
        self.assertEqual(merged[0xD4139C].refs, ['src/A.cpp:29'])


if __name__ == '__main__':
    unittest.main()


class TagFormsTest(unittest.TestCase):
    def test_static_tag_indexes_a_global_as_data(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, 'WoW/Source/Client.cpp', '// STATIC: 0x00B2F9A0\nstatic int32_t s_gameInitialized;\n')
            write(root, 'Engine/A.cpp', '    // STATIC: 0x00B414E8\n    static CVar* s_var;\n')
            index = addrmap.index_offsets(root)
            self.assertEqual(index[0xB2F9A0][0], 's_gameInitialized')
            self.assertEqual(index[0xB414E8][0], 's_var')
            self.assertEqual(addrmap.index_kinds(root)[0xB2F9A0], 'data')

    def test_bracketed_offset_list_indexes_every_instance(self):
        with tempfile.TemporaryDirectory() as root:
            write(root, 'a.h', '// OFFSET: [0x007D4920<CGUnit_C>, 0x007E55F0<CGPlayer_C>]\nT* Get() {\n')
            index = addrmap.index_offsets(root)
            self.assertEqual(index[0x7D4920][0], 'Get')
            self.assertEqual(index[0x7E55F0][0], 'Get')
            self.assertEqual(addrmap.index_kinds(root)[0x7D4920], 'func')

    def test_build_marks_static_rows_mapped_data(self):
        with tempfile.TemporaryDirectory() as decomp, tempfile.TemporaryDirectory() as recon:
            write(decomp, 'a.cpp', '// STATIC: 0x00B6AA2C\nint32_t CGlueMgr::m_suspended;\n')
            write(recon, 'src/A.cpp', 'if (*reinterpret_cast<const uint32_t*>(0xB6AA2C) == 0) {}\n')
            row = addrmap.build(decomp, recon)[0xB6AA2C]
            self.assertEqual((row.kind, row.status, row.symbol), ('data', 'mapped', 'CGlueMgr::m_suspended'))
