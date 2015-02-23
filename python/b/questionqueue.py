from b.foreign.call import call_with_eh
from b.foreign.call import wrap_eh
from b.foreign.ref import decref
from b.foreign.ref import incref
from b.native import NativeObject
from b.question import QuestionBase
import b.abc as abc
import b.foreign.all as foreign
import ctypes

class QuestionQueue(object):
    __token = object()

    def __init__(self, token, pointer, owns_pointer):
        if token is not self.__token:
            raise Exception('Do not construct directly')
        self.__pointer = pointer
        self.__owns_pointer = owns_pointer
        self.__deallocated = False

    @classmethod
    def single_threaded(cls):
        pointer = ctypes.POINTER(foreign.QuestionQueue)()
        call_with_eh(
            foreign.question_queue_allocate_single_threaded,
            ctypes.byref(pointer),
        )
        return cls(
            cls.__token,
            pointer=pointer,
            owns_pointer=True,
        )

    @classmethod
    def with_kqueue(cls, kqueue, trigger_kevent):
        pointer = ctypes.POINTER(foreign.QuestionQueue)()
        call_with_eh(
            foreign.question_queue_allocate_with_kqueue,
            kqueue.fileno(),
            ctypes.byref(_ForeignKevent(
                ident=trigger_kevent.ident,
                filter=trigger_kevent.filter,
                flags=trigger_kevent.flags,
                fflags=trigger_kevent.fflags,
                data=trigger_kevent.data,
                udata=trigger_kevent.udata,
            )),
            ctypes.byref(pointer),
        )
        return cls(
            cls.__token,
            pointer=pointer,
            owns_pointer=True,
        )

    @classmethod
    def from_pointer(cls, pointer):
        raise NotImplementedError()

    def enqueue(self, question_queue_item):
        assert not self.__deallocated
        call_with_eh(
            foreign.question_queue_enqueue,
            self.__pointer,
            question_queue_item.to_pointer(),
        )

    def try_dequeue(self):
        assert not self.__deallocated
        queue_item_pointer \
            = ctypes.POINTER(foreign.QuestionQueueItem)()
        closed = ctypes.c_bool()
        call_with_eh(
            foreign.question_queue_try_dequeue,
            self.__pointer,
            ctypes.byref(queue_item_pointer),
            ctypes.byref(closed),
        )
        if queue_item_pointer:
            queue_item \
                = NativeQuestionQueueItem.from_pointer(
                    pointer=queue_item_pointer,
                )
        else:
            queue_item = None
        return (queue_item, closed.value)

    def close(self):
        assert not self.__deallocated
        raise NotImplementedError()

    def to_pointer(self):
        return self.__pointer

    def __del__(self):
        if self.__deallocated:
            # __del__ called twice. See
            # https://groups.google.com/forum/#!topic/comp.lang.python/KTrPQzo5WDs
            return
        self.__deallocated = True
        if self.__owns_pointer:
            call_with_eh(
                foreign.question_queue_deallocate,
                self.__pointer,
            )

class QuestionQueueItemBase(abc.ABC):
    @abc.abstractproperty
    def question(self):
        pass

    @abc.abstractmethod
    def on_answer(self, answer):
        pass

    @abc.abstractmethod
    def to_pointer(self):
        pass

class NativeQuestionQueueItem(
    QuestionQueueItemBase,
    NativeObject,
):
    __token = object()

    def __init__(self, token, pointer):
        if token is not self.__token:
            raise Exception('Call from_pointer')
        self.__pointer = pointer
        self.__deallocated = False
        question_class \
            = QuestionBase.class_from_vtable_pointer(
                pointer[0].question_vtable,
            )
        self.__question = question_class.from_pointer(
            pointer[0].question,
        )

    @property
    def question(self):
        assert not self.__deallocated
        return self.__question

    def on_answer(self, answer):
        assert not self.__deallocated
        if answer is None:
            answer_pointer \
                = ctypes.POINTER(foreign.Answer)()
        else:
            answer_pointer = answer.to_pointer()
        try:
            call_with_eh(
                self.__pointer[0].answer_callback,
                answer_pointer,
                self.__pointer,
            )
        except:
            # FIXME(strager)
            #answer_pointer.deallocate()
            raise

    @classmethod
    def from_pointer(cls, pointer):
        '''
        Transfers ownership of the foreign QuestionQueueItem
        pointer.
        '''
        return cls(token=cls.__token, pointer=pointer)

    def to_pointer(self):
        assert not self.__deallocated
        return self.__pointer

    def __del__(self):
        if self.__deallocated:
            # __del__ called twice. See
            # https://groups.google.com/forum/#!topic/comp.lang.python/KTrPQzo5WDs
            return
        self.__deallocated = True
        raise NotImplementedError()
        call_with_eh(
            self.__pointer[0].deallocate,
            self.__pointer,
        )

class QuestionQueueItem(QuestionQueueItemBase, abc.ABC):
    def __init__(self, question):
        self.__question = question

        foreign_queue_item = foreign.QuestionQueueItem(
            deallocate=foreign.QuestionQueueItemDeallocate(
                self.__raw_deallocate,
            ),
            question=self.__question.to_pointer(),
            question_vtable
                =self.__question.vtable_pointer(),
            answer_callback=foreign.AnswerCallback(
                self.__raw_answer_callback,
            ),
        )
        self.__queue_item_pointer = ctypes.pointer(
            _PythonForeignQuestionQueueItem(
                foreign_queue_item=foreign_queue_item,
                python_queue_item=ctypes.py_object(self),
            ),
        )

    @classmethod
    def __raw_deallocate(cls, queue_item, eh):
        def f():
            queue_item_pointer = ctypes.cast(
                queue_item,
                ctypes.POINTER(
                    _PythonForeignQuestionQueueItem,
                ),
            )
            decref(queue_item_pointer[0].python_queue_item)
        return wrap_eh(f, eh)

    @classmethod
    def __raw_answer_callback(
        cls,
        answer_pointer,
        opaque,
        eh,
    ):
        def f():
            queue_item_pointer = ctypes.cast(
                opaque,
                ctypes.POINTER(
                    _PythonForeignQuestionQueueItem,
                ),
            )
            queue_item \
                = queue_item_pointer[0].python_queue_item
            if answer_pointer:
                answer = queue_item \
                    .__question.answer_class() \
                    .from_pointer(answer_pointer)
            else:
                answer = None
            queue_item.on_answer(answer)
        return wrap_eh(f, eh)

    @property
    def question(self):
        return self.__question

    @abc.abstractmethod
    def on_answer(self, answer):
        pass

    def to_pointer(self):
        incref(self)
        return ctypes.cast(
            self.__queue_item_pointer,
            ctypes.POINTER(foreign.QuestionQueueItem),
        )

class _PythonForeignQuestionQueueItem(ctypes.Structure):
    _fields_ = [
        # foreign_queue_item must be first, because pointers
        # to this type are cast to pointers to
        # QuestionQueueItem-s, and vice versa.
        ('foreign_queue_item', foreign.QuestionQueueItem),
        ('python_queue_item', ctypes.py_object),
    ]

class _ForeignKevent(ctypes.Structure):
    '''
    struct kevent, from BSD <sys/event.h>.
    '''
    _fields_ = [
        ('ident', ctypes.c_void_p),  # uintptr_t
        ('filter', ctypes.c_int16),
        ('flags', ctypes.c_uint16),
        ('fflags', ctypes.c_uint32),
        ('data', ctypes.c_void_p),  # intptr_t
        ('udata', ctypes.c_void_p),
    ]
