from b.foreign.Error import ErrorHandler
from b.foreign.core import b_export_func
from ctypes import POINTER
import ctypes

deallocate = b_export_func('b_deallocate', args=[
    ('pointer', ctypes.c_void_p),
    ('eh', POINTER(ErrorHandler)),
])
