from b.foreign.Error import ErrorHandler
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
import ctypes

class ByteSink(ctypes.Structure): pass

ByteSinkWriteBytes = b_func(argtypes=[
    POINTER(ByteSink),
    POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    POINTER(ErrorHandler),
])

ByteSinkDeallocate = b_func(argtypes=[
    POINTER(ByteSink),
    POINTER(ErrorHandler),
])

ByteSink._fields_ = [
    ('write_bytes', ByteSinkWriteBytes),
    ('deallocate', ByteSinkDeallocate),
]

serialize_1 = b_export_func('b_serialize_1', args=[
    POINTER(ByteSink),
    ctypes.c_uint8,
    POINTER(ErrorHandler),
])

serialize_2_be = b_export_func('b_serialize_2_be', args=[
    POINTER(ByteSink),
    ctypes.c_uint16,
    POINTER(ErrorHandler),
])

serialize_4_be = b_export_func('b_serialize_4_be', args=[
    POINTER(ByteSink),
    ctypes.c_uint32,
    POINTER(ErrorHandler),
])

serialize_8_be = b_export_func('b_serialize_8_be', args=[
    POINTER(ByteSink),
    ctypes.c_uint64,
    POINTER(ErrorHandler),
])

serialize_bytes = b_export_func('b_serialize_bytes', args=[
    POINTER(ByteSink),
    POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
    POINTER(ErrorHandler),
])

serialize_data_and_size_8_be = b_export_func(
    'b_serialize_data_and_size_8_be',
    args=[
        POINTER(ByteSink),
        POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        POINTER(ErrorHandler),
    ],
)
