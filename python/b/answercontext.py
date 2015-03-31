from b.foreign.call import call_with_eh
from b.foreign.call import wrap_eh
from b.foreign.ref import incref
from b.foreign.util import format_pointer
from b.foreign.util import weak_py_object
from b.gen import Future
from b.gen import gen_return
from b.question import QuestionBase
from ctypes import POINTER
import b.foreign.all as foreign
import ctypes
import logging

logger = logging.getLogger(__name__)

class AnswerContext(object):
    __token = object()

    def __init__(
        self,
        token,
        pointer_factory,
        question,
        process_controller,
        owns_pointer,
    ):
        if token is not self.__token:
            raise Exception('Use create or from_pointer')
        self.__pointer = pointer_factory(self)
        self.__question = question
        self.__process_controller = process_controller
        self.__owns_pointer = owns_pointer

    @classmethod
    def create(
        cls,
        question,
        question_queue,
        database,
        process_controller,
    ):
        '''
        Returns a pair of (AnswerContext, Future). The
        Future is resolved when gen_success,
        gen_success_answer, or gen_error is called.
        '''
        answer_future = Future()
        def pointer_factory(self):
            question_pointer = question.to_pointer()
            try:
                return ctypes.pointer(foreign.AnswerContext(
                    question=question_pointer,
                    question_vtable
                        =question.vtable_pointer(),
                    answer_callback
                        =cls.__raw_answer_callback_factory(
                            self=self,
                            answer_class
                                =question.answer_class(),
                            future=answer_future,
                        ),
                    answer_callback_opaque
                        =ctypes.c_void_p(),
                    question_queue
                        =question_queue.to_pointer(),
                    database=database.to_pointer()
                        if database
                        else None,
                ))
            finally:
                question._deallocate(question_pointer)
        ac = cls(
            token=cls.__token,
            pointer_factory=pointer_factory,
            question=question,
            process_controller=process_controller,
            owns_pointer=True,
        )
        return (ac, answer_future)

    @staticmethod
    def __raw_answer_callback_factory(
        self,
        answer_class,
        future,
    ):
        '''
        Returns a foreign.AnswerCallback which calls the
        given answer_callback with an AnswerBase object.

        This is a separate method to minimize the number of
        objects retained.
        '''
        self_list = [self]
        del self
        def raw_answer_callback(
            answer_pointer,
            _opaque,
            eh,
        ):
            def f():
                answer = answer_class.from_pointer(
                    answer_pointer,
                )
                self = self_list.pop()
                try:
                    future.resolve(answer)
                finally:
                    self.__dealloc()
            return wrap_eh(f, eh)
        return foreign.AnswerCallback(raw_answer_callback)

    @classmethod
    def from_pointer(cls, pointer, process_controller):
        question_class \
            = QuestionBase.class_from_vtable_pointer(
                pointer.contents.question_vtable,
            )
        question = question_class.from_pointer(
            pointer.contents.question,
        )
        return cls(
            token=cls.__token,
            pointer_factory=lambda self: pointer,
            question=question,
            process_controller=process_controller,
            owns_pointer=False,
        )

    @property
    def question(self):
        return self.__question

    @property
    def process_controller(self):
        return self.__process_controller

    def gen_need(self, questions):
        logger.debug('Need {}'.format(repr(questions)))
        question_pointers \
            = (POINTER(foreign.Question) * len(questions))()
        question_vtable_pointers \
            = (POINTER(foreign.QuestionVTable)
                * len(questions))()
        try:
            for (i, question) in enumerate(questions):
                question_pointers[i] = question.to_pointer()
                question_vtable_pointers[i] \
                    = question.vtable_pointer()

            future = Future()
            callbacks = _NeedCallbacks(
                future=future,
                questions=questions,
            )

            call_with_eh(
                foreign.answer_context_need,
                answer_context=self.__pointer,
                questions=question_pointers,
                question_vtables=question_vtable_pointers,
                questions_count=len(questions),
                completed_callback
                    =callbacks.completed_callback,
                cancelled_callback
                    =callbacks.cancelled_callback,
                callback_opaque=None,
            )
            result = yield future
        finally:
            pass
            #for (i, question) in enumerate(questions):
            #    question._dealloate(question_pointers[i])
        del callbacks
        yield gen_return(result)

    def gen_success(self):
        call_with_eh(
            foreign.answer_context_success,
            answer_context=self.__pointer,
        )
        yield gen_return(None)

    def gen_success_answer(self, answer):
        call_with_eh(
            foreign.answer_context_success_answer,
            answer_context=self.__pointer,
            answer=answer.to_pointer(),
        )
        yield gen_return(None)

    def gen_error(self):
        call_with_eh(
            foreign.answer_context_error,
            answer_context=self.__pointer,
        )
        yield gen_return(None)

    def __dealloc(self):
        assert self.__owns_pointer
        #self.__question._deallocate(
        #    self.__pointer[0].question,
        #)
        self.__pointer = None

    def __del__(self):
        print('__del__({})'.format(self))
        if self.__owns_pointer:
            self.__dealloc()

class _NeedCallbacks(object):
    def __init__(self, future, questions):
        self.__future = future
        self.__completed_callback \
            = foreign.NeedCompletedCallback(self.completed)
        self.__cancelled_callback \
            = foreign.NeedCancelledCallback(self.cancelled)
        self.__answer_classes = [
            q.answer_class() for q in questions
        ]
        logger.debug('Init {}'.format(repr(self)))

    def __repr__(self):
        return ('{}(completed_callback={}, '
            'cancelled_callback={})').format(
                self.__class__.__name__,
                format_pointer(self.__completed_callback),
                format_pointer(self.__cancelled_callback),
            )

    def completed(self, answer_pointers, _opaque, _eh):
        # TODO(strager): Use eh.
        self.__gc_callbacks()
        answers = []
        for (i, answer_class) \
                in enumerate(self.__answer_classes):
            answers.append(answer_class.from_pointer(
                answer_pointers[i],
            ))
        self.__future.resolve(answers)
        return True

    def cancelled(self, _opaque, _eh):
        # TODO(strager): Use eh.
        self.__gc_callbacks()
        self.__future.resolve(None)
        return True

    @property
    def completed_callback(self):
        return self.__completed_callback

    @property
    def cancelled_callback(self):
        return self.__cancelled_callback

    def __gc_callbacks(self):
        logger.debug('GC {}'.format(repr(self)))
        self.__completed_callback = None
        self.__cancelled_callback = None

    def __del__(self):
        logger.debug('del {}'.format(repr(self)))
