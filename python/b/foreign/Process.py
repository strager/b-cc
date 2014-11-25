import ctypes

from b.foreign.Error import ErrorHandler
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
from ctypes import c_char_p
from ctypes import c_void_p

class ProcessController(ctypes.Structure):
    # Opaque.
    pass

ProcessExitStatusType = ctypes.c_int
B_PROCESS_EXIT_STATUS_SIGNAL = 1
B_PROCESS_EXIT_STATUS_CODE = 2
B_PROCESS_EXIT_STATUS_EXCEPTION = 3

class ProcessExitStatusSignal(ctypes.Structure):
    _fields_ = [
        ('signal_number', ctypes.c_int),
    ]

class ProcessExitStatusCode(ctypes.Structure):
    _fields_ = [
        ('exit_code', ctypes.c_int64),
    ]

class ProcessExitStatusException(ctypes.Structure):
    _fields_ = [
        ('code', ctypes.c_uint32),
    ]

class ProcessExitStatusUnion(ctypes.Union):
    _fields_ = [
        ('signal', ProcessExitStatusSignal),
        ('code', ProcessExitStatusCode),
        ('exception', ProcessExitStatusException),
    ]

class ProcessExitStatus(ctypes.Structure):
    _anonymous_ = 'u'
    _fields_ = [
        ('type', ProcessExitStatusType),
        ('u', ProcessExitStatusUnion),
    ]

ProcessExitCallback = b_func(argtypes=[
    POINTER(ProcessExitStatus),
    c_void_p,
    POINTER(ErrorHandler),
])

process_controller_exec_basic = b_export_func(
    'b_process_controller_exec_basic',
    args=[
        ('process_controller', POINTER(ProcessController)),
        ('args', POINTER(c_char_p)),
        ('callback', ProcessExitCallback),
        ('callback_opaque', c_void_p),
        ('eh', POINTER(ErrorHandler)),
    ],
)
