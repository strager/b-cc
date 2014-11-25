from b.foreign.AnswerContext import AnswerContext
from b.foreign.Database import Database
from b.foreign.Error import ErrorHandler
from b.foreign.QuestionQueue import QuestionQueue
from b.foreign.QuestionQueue import QuestionQueueItem
from b.foreign.core import b_export_func
from ctypes import POINTER

question_dispatch_one = b_export_func(
    'b_question_dispatch_one',
    args=[
        ('question_queue_item', POINTER(QuestionQueueItem)),
        ('question_queue', POINTER(QuestionQueue)),
        ('database', POINTER(Database)),
        ('out', POINTER(POINTER(AnswerContext))),
        ('eh', POINTER(ErrorHandler)),
    ],
)
