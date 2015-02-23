import ctypes
import unittest

def format_pointer(pointer):
    address = ctypes.cast(pointer, ctypes.c_void_p).value
    if address is None:
        return 'NULL'
    else:
        return '{:#018x}'.format(address)

class TestUtil(unittest.TestCase):
    def test_format_pointer(self):
        self.assertEqual(
            'NULL',
            format_pointer(ctypes.c_void_p()),
        )
        self.assertEqual(
            '0x1234567a',
            format_pointer(ctypes.c_void_p(0x1234567A)),
        )

if __name__ == '__main__':
    unittest.main()
