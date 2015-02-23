import ctypes
import sys
import unittest

if not hasattr(ctypes, 'pythonapi'):
    raise ImportError()

_incref = ctypes.CFUNCTYPE(None, ctypes.py_object)(
    ('Py_IncRef', ctypes.pythonapi),
)

_decref = ctypes.CFUNCTYPE(None, ctypes.py_object)(
    ('Py_DecRef', ctypes.pythonapi),
)

def incref(object):
    _incref(ctypes.py_object(object))

def decref(object):
    _decref(ctypes.py_object(object))

class TestRef(unittest.TestCase):
    def test_ref_count_of_local(self):
        o = object()
        ref_count = sys.getrefcount(o)
        self.assertEqual(2, ref_count)

    def test_ref_count_of_local_2(self):
        o = object()
        o2 = o
        ref_count = sys.getrefcount(o)
        self.assertEqual(3, ref_count)
        del o2  # Fix lint.

    def test_incref(self):
        o = object()
        old_ref_count = sys.getrefcount(o)
        incref(o)
        new_ref_count = sys.getrefcount(o)
        self.assertEqual(old_ref_count + 1, new_ref_count)

    def test_decref(self):
        o = object()
        o2 = o
        old_ref_count = sys.getrefcount(o)
        decref(o)
        new_ref_count = sys.getrefcount(o)
        self.assertEqual(old_ref_count - 1, new_ref_count)
        incref(o)  # Don't crash!
        del o2  # Fix lint.

if __name__ == '__main__':
    unittest.main()
