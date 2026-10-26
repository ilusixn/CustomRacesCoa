"""x64 number conversion guard: a Lua number must reach a 32-bit (or smaller) integer through 64 bits.

The 32-bit client converts with __ftol2 / "fistp qword" in truncation mode and keeps the low dword, so numbers past
2^31 wrap (0xFFFFF800 stays 0xFFFFF800). A direct static_cast<int32_t>(double) on x64 yields 0x80000000 instead --
that broke Ascension's bit.contains(Enum.ClassMask.COA, ...) and LootCollector's realm mode.
"""
import os
import re
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
DIRECT = re.compile(r'static_cast<(int32_t|int|int16_t|int8_t|uint16_t|uint8_t|short|char)>\(\s*'
                    r'(FrameScript_GetNumber|lua_tonumber|luaL_checknumber|luaL_optnumber|FrameScript_ToNumber)\b')


class X64CastTest(unittest.TestCase):
    def test_no_direct_lua_number_to_int32_casts(self):
        hits = []
        for sub in ('WoW/Source', 'Engine/Source'):
            for dp, _, files in os.walk(os.path.join(ROOT, sub)):
                for f in files:
                    if f.endswith(('.cpp', '.c')):
                        path = os.path.join(dp, f)
                        with open(path, encoding='utf-8', errors='ignore') as fh:
                            for n, line in enumerate(fh, 1):
                                if DIRECT.search(line):
                                    hits.append(f'{os.path.relpath(path, ROOT)}:{n}')
        self.assertEqual(hits, [], 'cast through static_cast<int64_t> first:\n' + '\n'.join(hits[:20]))


if __name__ == '__main__':
    unittest.main()
