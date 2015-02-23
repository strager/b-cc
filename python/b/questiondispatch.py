from b.foreign.call import call_with_eh
from b.answercontext import AnswerContext
import b.foreign.all as foreign
import ctypes

def question_dispatch_one(
    queue_item,
    queue,
    database,
):
    answer_context_pointer \
        = ctypes.POINTER(foreign.AnswerContext)()
    call_with_eh(
        foreign.question_dispatch_one,
        queue_item.to_pointer(),
        queue.to_pointer(),
        database.to_pointer(),
        ctypes.byref(answer_context_pointer),
    )
    return AnswerContext.from_pointer(
        answer_context_pointer,
        process_controller=None,
    )
