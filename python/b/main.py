from b.database import Database
from b.foreign.call import call_with_eh
from b.foreign.call import wrap_eh
from b.answercontext import AnswerContext
from b.process import ProcessController
from ctypes import POINTER
import b.foreign.all as foreign
import b.gen
import ctypes

class Main(object):
    def __init__(self):
        pointer = POINTER(foreign.Main)()
        call_with_eh(
            foreign.main_allocate,
            out=ctypes.byref(pointer),
        )
        self.__pointer = pointer
        self.__deallocated = False

        # Lazily initialized.
        self.__process_controller = None

    @property
    def process_controller(self):
        assert not self.__deallocated
        # TODO(strager): Support multi-threading.
        if self.__process_controller is not None:
            return self.__process_controller
        pointer = POINTER(foreign.ProcessController)()
        call_with_eh(
            foreign.main_process_controller,
            main=self.__pointer,
            out=ctypes.byref(pointer),
        )
        self.__process_controller \
            = ProcessController(pointer)
        return self.__process_controller

    def loop(
        self,
        question,
        database,
        gen_dispatch_callback,
    ):
        assert not self.__deallocated

        def raw_dispatch_callback(
            answer_context_pointer,
            _main_closure,
            eh,
        ):
            def f():
                ac = AnswerContext.from_pointer(
                    pointer=answer_context_pointer,
                    process_controller \
                        =self.process_controller,
                )
                b.gen.loop(gen_dispatch_callback(ac))
            return wrap_eh(f, eh)
        dispatch_callback_pointer \
            = foreign.QuestionDispatchCallback(
                raw_dispatch_callback,
            )
        answer_pointer = POINTER(foreign.Answer)()
        call_with_eh(
            foreign.main_loop,
            main=self.__pointer,
            question=question.to_pointer(),
            question_vtable=question.vtable_pointer(),
            answer=ctypes.byref(answer_pointer),
            database=database.to_pointer(),
            dispatch_callback=dispatch_callback_pointer,
            dispatch_callback_opaque=None,
        )
        if answer_pointer:
            return question.answer_class() \
                .from_pointer(answer_pointer)
        else:
            return None

    def __del__(self):
        if self.__deallocated:
            # __del__ called twice. See
            # https://groups.google.com/forum/#!topic/comp.lang.python/KTrPQzo5WDs
            return

        self.__deallocated = True
        call_with_eh(
            foreign.main_deallocate,
            self.__pointer,
        )
        # No need to clean up self.__process_controller.
        # It's an unowned reference which main_deallocate
        # cleans up.

def list_dispatch_callback(dispatchers):
    dispatchers = list(dispatchers)
    def gen_dispatch(ac):
        for dispatcher in dispatchers:
            dispatched = yield \
                dispatcher.gen_dispatch_question(ac)
            if dispatched:
                return
        raise Exception(
            'No dispatcher for question {}'
                .format(repr(ac.question)),
        )
    return gen_dispatch

def main(
    question,
    question_classes,
    dispatch_callback,
    database_sqlite_path,
):
    database = Database.load_sqlite(
        path=database_sqlite_path,
        flags = Database.SQLITE_OPEN_CREATE \
            | Database.SQLITE_OPEN_READWRITE,
    )
    database.recheck_all(question_classes)

    main = Main()
    return main.loop(
        question=question,
        database=database,
        gen_dispatch_callback=dispatch_callback,
    )
