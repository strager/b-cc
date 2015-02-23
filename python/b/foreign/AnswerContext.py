from b.foreign.Database import Database
from b.foreign.Error import ErrorHandler
from b.foreign.QuestionAnswer import Answer
from b.foreign.QuestionAnswer import AnswerCallback
from b.foreign.QuestionAnswer import Question
from b.foreign.QuestionAnswer import QuestionVTable
from b.foreign.QuestionQueue import QuestionQueue
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
from ctypes import c_void_p
import ctypes

class AnswerContext(ctypes.Structure):
    _fields_ = [
        ('question', POINTER(Question)),
        ('question_vtable', POINTER(QuestionVTable)),
        ('answer_callback', AnswerCallback),
        ('answer_callback_opaque', c_void_p),
        ('question_queue', POINTER(QuestionQueue)),
        ('database', POINTER(Database)),
    ]

NeedCompletedCallback = b_func(argtypes=[
    POINTER(POINTER(Answer)),
    c_void_p,
    POINTER(ErrorHandler),
])

NeedCancelledCallback = b_func(argtypes=[
    c_void_p,
    POINTER(ErrorHandler),
])

answer_context_need = b_export_func(
    'b_answer_context_need',
    args=[
        ('answer_context', POINTER(AnswerContext)),
        ('questions', POINTER(POINTER(Question))),
        (
            'question_vtables',
            POINTER(POINTER(QuestionVTable)),
        ),
        ('questions_count', ctypes.c_size_t),
        ('completed_callback', NeedCompletedCallback),
        ('cancelled_callback', NeedCancelledCallback),
        ('callback_opaque', c_void_p),
        ('eh', POINTER(ErrorHandler)),
    ],
)

answer_context_success = b_export_func(
    'b_answer_context_success',
    args=[
        ('answer_context', POINTER(AnswerContext)),
        ('eh', POINTER(ErrorHandler)),
    ],
)

answer_context_success_answer = b_export_func(
    'b_answer_context_success_answer',
    args=[
        ('answer_context', POINTER(AnswerContext)),
        ('answer', POINTER(Answer)),
        ('eh', POINTER(ErrorHandler)),
    ],
)

answer_context_error = b_export_func(
    'b_answer_context_error',
    args=[
        ('answer_context', POINTER(AnswerContext)),
        ('eh', POINTER(ErrorHandler)),
    ],
)
