from b.foreign.util import format_pointer
from b.foreign.util import weak_py_object
import ctypes
import sys
import unittest

class TestUtil(unittest.TestCase):
    def test_format_pointer(self):
        self.assertEqual(
            'NULL',
            format_pointer(ctypes.c_void_p()),
        )
        self.assertEqual(
            '0x000000001234567a',
            format_pointer(ctypes.c_void_p(0x1234567A)),
        )

    def test_weak_py_object(self):
        o = object()
        before = sys.getrefcount(o)
        o_weak = weak_py_object(o)
        after = sys.getrefcount(o)
        self.assertEqual(before, after)
        self.assertIs(o_weak.value, o)

if __name__ == '__main__':
    unittest.main()
