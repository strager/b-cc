import ctypes

from b.foreign.AnswerContext import AnswerContext
from b.foreign.Error import ErrorHandler
from b.foreign.Process import ProcessController
from b.foreign.QuestionAnswer import Answer
from b.foreign.QuestionAnswer import Question
from b.foreign.QuestionAnswer import QuestionVTable
from b.foreign.QuestionVTableSet import QuestionVTableSet
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
from ctypes import c_char_p
from ctypes import c_void_p

class MainClosure(ctypes.Structure):
    _fields_ = [
        ('process_controller', POINTER(ProcessController)),
        ('opaque', c_void_p),
    ]

QuestionDispatchCallback = b_func(argtypes=[
    POINTER(AnswerContext),
    POINTER(MainClosure),
    POINTER(ErrorHandler),
])

main = b_export_func('b_main', args=[
    ('question', POINTER(Question)),
    ('question_vtable', POINTER(QuestionVTable)),
    ('answer', POINTER(POINTER(Answer))),
    ('database_sqlite_path', c_char_p),
    ('question_vtable_set', POINTER(QuestionVTableSet)),
    ('dispatch_callback', QuestionDispatchCallback),
    ('dispatch_callback_opaque', c_void_p),
    ('eh', POINTER(ErrorHandler)),
])
