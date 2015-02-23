from b.bytesink import ByteSink
from b.bytesource import ByteSource
from b.foreign.call import call_with_eh
from b.foreign.call import wrap_eh
from b.foreign.ref import decref
from b.foreign.ref import incref
from b.foreign.util import format_pointer
from b.native import NativeObject
from b.uuid import UUID
from ctypes import POINTER
import b.abc as abc
import b.foreign.all as foreign
import ctypes

class QuestionBase(NativeObject, abc.ABC):
    @classmethod
    def yield_all_classes(cls):
        '''
        Yields all QuestionBase classes (i.e. classes which
        have QuestionBase as an ancestor class).
        '''
        classes = []
        stack = [cls]
        # Assumes no cycles. Will infinite loop if there are
        # cycles.
        while stack:
            c = stack.pop()
            if not c.__abstractmethods__:
                classes.append(c)
            stack.extend(c.__subclasses__())
        return classes

    @classmethod
    def class_from_uuid(cls, uuid):
        for c in cls.yield_all_classes():
            if c.uuid() == uuid:
                return c
        raise IndexError('No QuestionBase class for UUID')

    @classmethod
    def class_from_vtable_pointer(cls, vtable_pointer):
        return cls.class_from_uuid(UUID.from_foreign(
            vtable_pointer.contents.uuid,
        ))

    @abc.abstractmethod
    #@staticmethod
    def uuid():
        pass

    @abc.abstractmethod
    #@staticmethod
    def answer_class():
        pass

    @abc.abstractmethod
    #@staticmethod
    def vtable_pointer():
        pass

    @classmethod
    def replicate_pointer(cls, pointer):
        question_pointer = POINTER(foreign.Question)()
        call_with_eh(
            cls.vtable_pointer().contents.replicate,
            pointer,
            ctypes.byref(question_pointer),
        )
        return question_pointer

    def query_answer(self):
        question_vtable = self.vtable_pointer().contents
        answer_vtable \
            = question_vtable.answer_vtable.contents

        question_pointer = self.to_pointer()
        try:
            answer_pointer = POINTER(foreign.Answer)()
            call_with_eh(
                question_vtable.answer,
                question_pointer,
                ctypes.byref(answer_pointer),
            )
            try:
                return self.answer_class().from_pointer(
                    answer_pointer,
                )
            finally:
                call_with_eh(
                    answer_vtable.deallocate,
                    answer_pointer,
                )
        finally:
            call_with_eh(
                question_vtable.deallocate,
                question_pointer,
            )

class AnswerBase(NativeObject, abc.ABC):
    @abc.abstractmethod
    #@staticmethod
    def question_class():
        pass

    @classmethod
    def vtable_pointer(cls):
        return cls.question_class().vtable_pointer() \
            .contents.answer_vtable

    @classmethod
    def replicate_pointer(cls, pointer):
        answer_pointer = POINTER(foreign.Answer)()
        call_with_eh(
            cls.vtable_pointer().contents.replicate,
            pointer,
            ctypes.byref(answer_pointer),
        )
        return answer_pointer

class _NativeQuestionAnswerMixin(abc.ABC):
    __token = object()

    def __init__(self, token, pointer):
        if token is not _NativeQuestionAnswerMixin.__token:
            raise Exception('Use from_pointer')
        self.__pointer = pointer

    @property
    def pointer(self):
        return self.__pointer

    @classmethod
    def from_pointer(cls, pointer):
        return cls(
            _NativeQuestionAnswerMixin.__token,
            cls.replicate_pointer(pointer),
        )

    def to_pointer(self):
        return self.replicate_pointer(self.__pointer)

    def __repr__(self):
        return '{}({})'.format(
            self.__class__.__name__,
            format_pointer(self.__pointer),
        )

class NativeQuestion(
    _NativeQuestionAnswerMixin,
    QuestionBase,
):
    @classmethod
    def uuid(cls):
        return UUID.from_foreign(
            cls.vtable_pointer().contents.uuid,
        )

class NativeAnswer(_NativeQuestionAnswerMixin, AnswerBase):
    pass

class _PythonNativeMixin(abc.ABC):
    '''
    Maintains a set of ctypes pointers refering to instances
    of subclasses of this class.

    Call self.to_pointer() to get a Question pointer which
    can be given to the foreign interface.  Calling
    self.to_pointer() increases self's ref count.  Call
    cls.from_pointer(self) to go back to self.  Call
    self._deallocate(p) to free the given pointer,
    decreasing self's ref count..
    '''

    @classmethod
    def from_pointer(cls, pointer):
        object = ctypes.cast(pointer, ctypes.py_object) \
            .value
        incref(object)
        return object

    @abc.abstractmethod
    #@staticmethod
    def _foreign_pointer_class():
        pass

    def to_pointer(self):
        pointer = ctypes.cast(
            ctypes.c_void_p(id(self)),
            POINTER(self._foreign_pointer_class()),
        )
        incref(self)
        return pointer

    @classmethod
    def _deallocate(cls, pointer):
        return cls.from_pointer(pointer) \
            .__deallocate(pointer)

    def __deallocate(self, pointer):
        assert ctypes.cast(pointer, ctypes.c_void_p).value \
            == id(self)
        decref(self)

class Question(_PythonNativeMixin, QuestionBase, abc.ABC):
    @staticmethod
    def _foreign_pointer_class():
        return foreign.Question

    # TODO(strager): Make thread-safe.
    __vtable_pointer = None

    # TODO(strager): Use a memoize decorator.
    @classmethod
    def vtable_pointer(cls):
        if cls.__vtable_pointer is not None:
            return cls.__vtable_pointer
        vtable = foreign.QuestionVTable(
            uuid=cls.uuid().to_foreign(),
            answer_vtable=cls.answer_class() \
                .vtable_pointer(),
            answer=foreign.QuestionVTableAnswer(
                cls.__raw_answer,
            ),
            equal=foreign.QuestionVTableEqual(
                cls.__raw_equal,
            ),
            replicate=foreign.QuestionVTableReplicate(
                cls.__raw_replicate,
            ),
            deallocate=foreign.QuestionVTableDeallocate(
                cls.__raw_deallocate,
            ),
            serialize=foreign.QuestionVTableSerialize(
                cls.__raw_serialize,
            ),
            deserialize=foreign.QuestionVTableDeserialize(
                cls.__raw_deserialize,
            ),
        )
        cls.__vtable_pointer \
            = POINTER(foreign.QuestionVTable)(vtable)
        return cls.__vtable_pointer

    @classmethod
    def __raw_answer(
        cls,
        question_pointer,
        out_pointer,
        eh,
    ):
        def f():
            question = cls.from_pointer(question_pointer)
            out_pointer[0] \
                = question.query_answer().to_pointer()
        return wrap_eh(f, eh)

    @classmethod
    def __raw_equal(
        cls,
        a_pointer,
        b_pointer,
        out_pointer,
        eh,
    ):
        def f():
            a = cls.from_pointer(a_pointer)
            b = cls.from_pointer(b_pointer)
            out_pointer[0] = a == b
        return wrap_eh(f, eh)

    @classmethod
    def __raw_replicate(
        cls,
        question_pointer,
        out_pointer,
        eh,
    ):
        def f():
            question = cls.from_pointer(question_pointer)
            out_pointer[0] = question.to_pointer()
        return wrap_eh(f, eh)

    @classmethod
    def __raw_deallocate(cls, question_pointer, eh):
        def f():
            cls._deallocate(question_pointer)
        return wrap_eh(f, eh)

    @classmethod
    def __raw_serialize(
        cls,
        question_pointer,
        byte_sink_pointer,
        eh,
    ):
        def f():
            question = cls.from_pointer(question_pointer)
            byte_sink \
                = ByteSink.from_pointer(byte_sink_pointer)
            question.serialize(byte_sink)
        return wrap_eh(f, eh)

    @classmethod
    def __raw_deserialize(
        cls,
        byte_source_pointer,
        out_pointer,
        eh,
    ):
        def f():
            byte_source = ByteSource.from_pointer(
                byte_source_pointer,
            )
            question = cls.deserialize(byte_source)
            out_pointer[0] = question.to_pointer()
            return True
        return wrap_eh(f, eh)

    @abc.abstractmethod
    #@staticmethod
    def uuid():
        pass

    @abc.abstractmethod
    #@staticmethod
    def answer_class():
        pass

    @abc.abstractmethod
    def query_answer(self):
        pass

    @abc.abstractmethod
    def __eq__(self, other):
        pass

    @abc.abstractmethod
    def serialize(self, byte_sink):
        pass

    @abc.abstractmethod
    #@staticmethod
    def deserialize(byte_source):
        pass

class Answer(_PythonNativeMixin, AnswerBase, abc.ABC):
    @staticmethod
    def _foreign_pointer_class():
        return foreign.Answer

    # TODO(strager): Make thread-safe.
    __vtable_pointer = None

    # TODO(strager): Use a memoize decorator.
    @classmethod
    def vtable_pointer(cls):
        if cls.__vtable_pointer is not None:
            return cls.__vtable_pointer
        vtable = foreign.AnswerVTable(
            equal=foreign.AnswerVTableEqual(
                cls.__raw_equal,
            ),
            replicate=foreign.AnswerVTableReplicate(
                cls.__raw_replicate,
            ),
            deallocate=foreign.AnswerVTableDeallocate(
                cls.__raw_deallocate,
            ),
            serialize=foreign.AnswerVTableSerialize(
                cls.__raw_serialize,
            ),
            deserialize=foreign.AnswerVTableDeserialize(
                cls.__raw_deserialize,
            ),
        )
        cls.__vtable_pointer \
            = POINTER(foreign.AnswerVTable)(vtable)
        return cls.__vtable_pointer

    @classmethod
    def __raw_equal(
        cls,
        a_pointer,
        b_pointer,
        out_pointer,
        eh,
    ):
        def f():
            a = cls.from_pointer(a_pointer)
            b = cls.from_pointer(b_pointer)
            out_pointer[0] = a == b
        return wrap_eh(f, eh)

    @classmethod
    def __raw_replicate(
        cls,
        answer_pointer,
        out_pointer,
        eh,
    ):
        def f():
            answer = cls.from_pointer(answer_pointer)
            out_pointer[0] = answer.to_pointer()
        return wrap_eh(f, eh)

    @classmethod
    def __raw_deallocate(cls, answer_pointer, eh):
        def f():
            cls._deallocate(answer_pointer)
        return wrap_eh(f, eh)

    @classmethod
    def __raw_serialize(
        cls,
        answer_pointer,
        byte_sink_pointer,
        eh,
    ):
        def f():
            answer = cls.from_pointer(answer_pointer)
            byte_sink \
                = ByteSink.from_pointer(byte_sink_pointer)
            answer.serialize(byte_sink)
            return True
        return wrap_eh(f, eh)

    @classmethod
    def __raw_deserialize(
        cls,
        byte_source_pointer,
        out_pointer,
        eh,
    ):
        def f():
            byte_source = ByteSource.from_pointer(
                byte_source_pointer,
            )
            answer = cls.deserialize(byte_source)
            out_pointer[0] = answer.to_pointer()
            return True
        return wrap_eh(f, eh)

    @abc.abstractmethod
    def __eq__(self, other):
        pass

    @abc.abstractmethod
    def serialize(self, byte_sink):
        pass

    @abc.abstractmethod
    #@staticmethod
    def deserialize(byte_source):
        pass
