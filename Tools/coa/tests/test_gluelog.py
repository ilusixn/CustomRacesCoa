import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import gluelog  # noqa: E402


class GlueLogTest(unittest.TestCase):
    def test_strips_timestamps(self):
        self.assertEqual(gluelog.normalize('10/25 03:50:31.318  X\n'), 'X')

    def test_only_new_lines(self):
        self.assertEqual(gluelog.new_lines(['a', 'b'], ['a', 'c']), ['c'])

    def test_repeated_line_counts(self):
        self.assertEqual(gluelog.new_lines(['x'], ['x', 'x']), ['x'])


if __name__ == '__main__':
    unittest.main()
