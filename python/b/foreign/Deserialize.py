from b.foreign.Error import ErrorHandler
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
import ctypes

class ByteSource(ctypes.Structure): pass

ByteSourceReadBytes = b_func(argtypes=[
    POINTER(ByteSource),
    POINTER(ctypes.c_uint8),
    POINTER(ctypes.c_size_t),
    POINTER(ErrorHandler),
])

ByteSourceDeallocate = b_func(argtypes=[
    POINTER(ByteSource),
    POINTER(ErrorHandler),
])

ByteSource._fields_ = [
    ('read_bytes', ByteSourceReadBytes),
    ('deallocate', ByteSourceDeallocate),
]

deserialize_1 = b_export_func('b_deserialize_1', args=[
    POINTER(ByteSource),
    POINTER(ctypes.c_uint8),
    POINTER(ErrorHandler),
])

deserialize_2_be = b_export_func(
    'b_deserialize_2_be',
    args=[
        POINTER(ByteSource),
        POINTER(ctypes.c_uint16),
        POINTER(ErrorHandler),
    ],
)

deserialize_4_be = b_export_func(
    'b_deserialize_4_be',
    args=[
        POINTER(ByteSource),
        POINTER(ctypes.c_uint32),
        POINTER(ErrorHandler),
    ],
)

deserialize_8_be = b_export_func(
    'b_deserialize_8_be',
    args=[
        POINTER(ByteSource),
        POINTER(ctypes.c_uint64),
        POINTER(ErrorHandler),
    ],
)

deserialize_data_and_size_8_be = b_export_func(
    'b_deserialize_data_and_size_8_be',
    args=[
        POINTER(ByteSource),
        POINTER(POINTER(ctypes.c_uint8)),
        POINTER(ctypes.c_size_t),
        POINTER(ErrorHandler),
    ],
)
