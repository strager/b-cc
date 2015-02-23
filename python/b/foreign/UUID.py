from b.foreign.core import b_export_func
import ctypes

class UUID(ctypes.Structure):
    _fields_ = [
        ('data', ctypes.c_uint8 * 16),
    ]

    def __eq__(self, other):
        return uuid_equal(
            ctypes.byref(self), ctypes.byref(other))

uuid_equal = b_export_func('b_uuid_equal', args=[
    ctypes.POINTER(UUID),
    ctypes.POINTER(UUID),
], restype=ctypes.c_bool)
