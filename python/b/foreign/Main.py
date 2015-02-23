from b.foreign.AnswerContext import AnswerContext
from b.foreign.Database import Database
from b.foreign.Error import ErrorHandler
from b.foreign.Process import ProcessController
from b.foreign.QuestionAnswer import Answer
from b.foreign.QuestionAnswer import Question
from b.foreign.QuestionAnswer import QuestionVTable
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
from ctypes import c_void_p
import ctypes

class Main(ctypes.Structure):
    # Opaque.
    pass

QuestionDispatchCallback = b_func(argtypes=[
    POINTER(AnswerContext),
    c_void_p,
    POINTER(ErrorHandler),
])

main_allocate = b_export_func('b_main_allocate', args=[
    ('out', POINTER(POINTER(Main))),
    ('eh', POINTER(ErrorHandler)),
])

main_deallocate = b_export_func('b_main_deallocate', args=[
    ('main', POINTER(Main)),
    ('eh', POINTER(ErrorHandler)),
])

main_process_controller = b_export_func(
    'b_main_process_controller',
    args=[
        ('main', POINTER(Main)),
        ('out', POINTER(POINTER(ProcessController))),
        ('eh', POINTER(ErrorHandler)),
    ],
)

main_loop = b_export_func('b_main_loop', args=[
    ('main', POINTER(Main)),
    ('question', POINTER(Question)),
    ('question_vtable', POINTER(QuestionVTable)),
    ('database', POINTER(Database)),
    ('answer', POINTER(POINTER(Answer))),
    ('dispatch_callback', QuestionDispatchCallback),
    ('dispatch_callback_opaque', c_void_p),
    ('eh', POINTER(ErrorHandler)),
])
