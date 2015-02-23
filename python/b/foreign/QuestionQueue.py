from b.foreign.Error import ErrorHandler
from b.foreign.QuestionAnswer import AnswerCallback
from b.foreign.QuestionAnswer import Question
from b.foreign.QuestionAnswer import QuestionVTable
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
import ctypes

class QuestionQueue(ctypes.Structure):
    # Opaque.
    pass

class QuestionQueueItem(ctypes.Structure): pass

QuestionQueueItemDeallocate = b_func(argtypes=[
    POINTER(QuestionQueueItem),
    POINTER(ErrorHandler),
])
QuestionQueueItem._fields_ = [
    ('deallocate', QuestionQueueItemDeallocate),
    ('question', POINTER(Question)),
    ('question_vtable', POINTER(QuestionVTable)),
    ('answer_callback', AnswerCallback),
]

question_queue_allocate_single_threaded = b_export_func(
    'b_question_queue_allocate_single_threaded',
    args=[
        POINTER(POINTER(QuestionQueue)),
        POINTER(ErrorHandler),
    ],
)

question_queue_allocate_with_kqueue = b_export_func(
    'b_question_queue_allocate_with_kqueue',
    args=[
        ctypes.c_int,
        ctypes.c_void_p,
        POINTER(POINTER(QuestionQueue)),
        POINTER(ErrorHandler),
    ],
)

question_queue_enqueue = b_export_func(
    'b_question_queue_enqueue',
    args=[
        POINTER(QuestionQueue),
        POINTER(QuestionQueueItem),
        POINTER(ErrorHandler),
    ],
)

question_queue_try_dequeue = b_export_func(
    'b_question_queue_try_dequeue',
    args=[
        ('queue', POINTER(QuestionQueue)),
        ('out', POINTER(POINTER(QuestionQueueItem))),
        ('closed', POINTER(ctypes.c_bool)),
        ('eh', POINTER(ErrorHandler)),
    ],
)

question_queue_deallocate = b_export_func(
    'b_question_queue_deallocate',
    args=[
        POINTER(QuestionQueue),
        POINTER(ErrorHandler),
    ],
)
