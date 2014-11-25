import b.foreign.all as foreign
import ctypes

from b.foreign.QuestionAnswer import Question
from b.foreign.call import call_with_eh
from ctypes import POINTER
from ctypes import c_char_p
from ctypes import c_void_p

class FileAnswer(object):
    def __init__(self, pointer):
        self.__pointer = pointer

    @staticmethod
    def from_pointer(pointer):
        return FileAnswer(pointer)

    def __repr__(self):
        return '{}({:#016x})'.format(
            self.__class__.__name__,
            ctypes.cast(self.__pointer, c_void_p).value,
        )

class FileQuestion(object):
    def __init__(self, path):
        self.__path = path

    answer_class = FileAnswer

    @staticmethod
    def vtable_pointer():
        return foreign.file_contents_question_vtable()

    @staticmethod
    def from_pointer(pointer):
        path = ctypes.cast(pointer, c_char_p).value
        return FileQuestion(path)

    def pointer(self):
        orig_question = ctypes.cast(
            c_char_p(self.__path), POINTER(Question))
        question = POINTER(Question)()
        call_with_eh(
            FileQuestion.vtable_pointer().contents
                .replicate,
            orig_question,
            ctypes.byref(question),
        )
        return question

    @property
    def path(self):
        return self.__path

    def __repr__(self):
        return '{}({})'.format(
            self.__class__.__name__,
            repr(self.__path),
        )
