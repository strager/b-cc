import b.foreign.all as foreign
import b.gen
import ctypes

from b.foreign.call import call_with_eh
from b.support import AnswerContext
from b.support import ProcessController
from ctypes import POINTER

def list_dispatch_callback(dispatchers):
    dispatchers = list(dispatchers)
    def gen_dispatch(ac):
        for dispatcher in dispatchers:
            dispatched = yield \
                dispatcher.gen_dispatch_question(ac)
            if dispatched:
                return
        raise Exception('No dispatcher for question {}'
            .format(repr(ac.question)))
    return gen_dispatch

def main(
        question,
        question_classes,
        dispatch_callback,
        database_sqlite_path,
):
    vtable_set_pointer \
        = POINTER(foreign.QuestionVTableSet)()
    call_with_eh(
        foreign.question_vtable_set_allocate,
        ctypes.byref(vtable_set_pointer),
    )
    for question_class in question_classes:
        call_with_eh(
            foreign.question_vtable_set_add,
            vtable_set_pointer,
            question_class.vtable_pointer(),
        )

    def dispatch_callback_wrapper(
            answer_context,
            main_closure,
            eh,
    ):
        ac = AnswerContext(
            pointer=answer_context,
            question_classes=question_classes,
            process_controller=ProcessController(
                main_closure.contents.process_controller),
        )
        b.gen.loop(dispatch_callback(ac))
        return True

    answer_pointer = POINTER(foreign.Answer)()
    dispatch_callback_pointer \
        = foreign.QuestionDispatchCallback(
            dispatch_callback_wrapper)
    call_with_eh(
        foreign.main,
        question=question.pointer(),
        question_vtable=question.vtable_pointer(),
        answer=ctypes.byref(answer_pointer),
        database_sqlite_path=str(database_sqlite_path),
        question_vtable_set=vtable_set_pointer,
        dispatch_callback=dispatch_callback_pointer,
        dispatch_callback_opaque=None,
    )
    del dispatch_callback_pointer

    return question.answer_class.from_pointer(
        answer_pointer)
