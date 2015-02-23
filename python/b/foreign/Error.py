from b.foreign.core import b_func
from ctypes import POINTER
from ctypes import c_int
import ctypes

class Error(ctypes.Structure):
    _fields_ = [
        ('errno_value', c_int),
    ]

ErrorHandlerResult = c_int
B_ERROR_ABORT = 1
B_ERROR_RETRY = 2
B_ERROR_IGNORE = 3

class ErrorHandler(ctypes.Structure): pass

ErrorHandlerFunc = b_func(argtypes=[
    POINTER(ErrorHandler),
    Error,
], restype=ErrorHandlerResult)

ErrorHandler._fields_ = [
    ('f', POINTER(ErrorHandlerFunc)),
]
