import b.foreign.all as foreign
import ctypes
import logging
import signal

from b.foreign.call import call_with_eh
from b.gen import Future
from b.gen import gen_return
from ctypes import POINTER

logger = logging.getLogger(__name__)

def _pointer_repr(pointer):
    if pointer is None:
        return repr(pointer)
    return '{:#016x}'.format(ctypes.cast(pointer, ctypes.c_void_p).value)

def _signal_number_to_str(signal_number):
    for attr in dir(signal):
        if attr.startswith('SIG') and getattr(signal, attr) == signal_number:
            return attr
    return None

class ExitError(Exception):
    def __init__(self, exit_status):
        super(ExitError, self).__init__()
        self.__exit_status = exit_status

    def __str__(self):
        return 'Process exited with {}'.format(
            self.exit_status)

    @property
    def exit_status(self):
        return self.__exit_status

class ExitStatus(object):
    @property
    def is_fatal(self):
        # Subclasses should override.
        raise NotImplementedError()

class ExitSignal(object):
    def __init__(self, signal_number):
        self.__signal_number = signal_number

    @property
    def signal_number(self):
        return self.__signal_number

    @property
    def signal_string(self):
        s = _signal_number_to_str(self.signal_number)
        if s is None:
            return '#{}'.format(self.signal_number)
        return s

    @property
    def is_fatal(self):
        return True

    def __str__(self):
        return 'signal {}'.format(self.signal_string)

class ExitCode(object):
    def __init__(self, exit_code):
        self.__exit_code = exit_code

    @property
    def exit_code(self):
        return self.__exit_code

    @property
    def is_fatal(self):
        return self.exit_code != 0

    def __str__(self):
        return 'exit code {}'.format(self.exit_code)

class ExitException(object):
    def __init__(self, code):
        self.__code = code

    @property
    def code(self):
        return self.__code

    @property
    def is_fatal(self):
        return True

    def __str__(self):
        return 'exception {:#08x}'.format(self.code)

def _exit_status_from_pointer(pointer):
    type = pointer.contents.type
    if type == foreign.B_PROCESS_EXIT_STATUS_SIGNAL:
        return ExitSignal(
            pointer.contents.signal.signal_number)
    elif type == foreign.B_PROCESS_EXIT_STATUS_CODE:
        return ExitCode(pointer.contents.code.exit_code)
    elif type == foreign.B_PROCESS_EXIT_STATUS_EXCEPTION:
        return ExitException(
            pointer.contents.exception.code)
    else:
        raise ValueError(
            'Unkown exit status type {}'.format(type))

class _NeedCallbacks(object):
    def __init__(self, future):
        self.__future = future
        self.__completed_callback \
            = foreign.NeedCompletedCallback(self.completed)
        self.__cancelled_callback \
            = foreign.NeedCancelledCallback(self.cancelled)
        logger.debug('Init {}'.format(repr(self)))

    def __repr__(self):
        return ('{}(completed_callback={}, '
            'cancelled_callback={})').format(
                self.__class__.__name__,
                _pointer_repr(self.__completed_callback),
                _pointer_repr(self.__cancelled_callback),
            )

    def completed(self, answers, _opaque, _eh):
        self.__gc_callbacks()
        # TODO(strager): Pass answers.
        self.__future.resolve()
        return True

    def cancelled(self, _opaque, _eh):
        self.__gc_callbacks()
        self.__future.cancel()
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

class _ExecCallback(object):
    def __init__(self, future, throw_on_error):
        self.__future = future
        self.__callback \
            = foreign.ProcessExitCallback(self.callback)
        self.__throw_on_error = throw_on_error
        logger.debug('Init {}'.format(repr(self)))

    def __repr__(self):
        return '{}(callback={})'.format(
            self.__class__.__name__,
            _pointer_repr(self.__callback),
        )

    def callback(self, exit_status_pointer, opaque, eh):
        self.__gc()
        exit_status = _exit_status_from_pointer(
            exit_status_pointer)
        if self.__throw_on_error and exit_status.is_fatal:
            self.__future.throw(ExitError(exit_status))
        else:
            self.__future.resolve(exit_status)
        return True

    @property
    def pointer(self):
        return self.__callback

    def __gc(self):
        logger.debug('GC {}'.format(repr(self)))
        self.__callback = None

    def __del__(self):
        logger.debug('del {}'.format(repr(self)))

class AnswerContext(object):
    def __init__(
            self,
            pointer,
            question_classes,
            process_controller,
    ):
        self.__pointer = pointer
        self.__question = _question_from_pointer(
            question=pointer.contents.question,
            question_vtable= \
                pointer.contents.question_vtable,
            question_classes=question_classes,
        )
        self.__process_controller = process_controller

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
        for (i, question) in enumerate(questions):
            question_pointers[i] = question.pointer()
            question_vtable_pointers[i] \
                = question.vtable_pointer()

        future = Future()
        callbacks = _NeedCallbacks(future)

        call_with_eh(
            foreign.answer_context_need,
            answer_context=self.__pointer,
            questions=question_pointers,
            question_vtables=question_vtable_pointers,
            questions_count=len(questions),
            completed_callback=callbacks.completed_callback,
            cancelled_callback=callbacks.cancelled_callback,
            callback_opaque=None,
        )
        yield future
        del callbacks

    def gen_success(self):
        call_with_eh(
            foreign.answer_context_success,
            answer_context=self.__pointer,
        )
        yield gen_return(None)

def _question_from_pointer(
        question,
        question_vtable,
        question_classes,
):
    for question_class in question_classes:
        if question_class.vtable_pointer().contents.uuid \
                == question_vtable.contents.uuid:
            return question_class.from_pointer(question)
    raise Exception(
        'Could not find Python Question class for C '
        'QuestionVTable',
    )

class ProcessController(object):
    def __init__(self, pointer):
        self.__pointer = pointer

    def gen_check_exec_basic(self, args):
        future = Future()
        callback = _ExecCallback(
            future=future, throw_on_error=True)

        args_array = (ctypes.c_char_p * (len(args) + 1))()
        for (i, arg) in enumerate(args):
            args_array[i] = str(arg)
        args_array[len(args)] = None

        call_with_eh(
            foreign.process_controller_exec_basic,
            process_controller=self.__pointer,
            args=args_array,
            callback=callback.pointer,
            callback_opaque=None,
        )
        yield future
        del callback
