from b.question import NativeAnswer
from b.question import NativeQuestion
import b.foreign.all as foreign
import ctypes

class FileAnswer(NativeAnswer):
    @staticmethod
    def question_class():
        return FileQuestion

class FileQuestion(NativeQuestion):
    @staticmethod
    def answer_class():
        return FileAnswer

    @staticmethod
    def vtable_pointer():
        return foreign.file_contents_question_vtable()

    @classmethod
    def from_path(cls, path):
        return cls.from_pointer(ctypes.cast(
            ctypes.c_char_p(path.encode('utf8')),
            ctypes.POINTER(foreign.Question),
        ))

    @property
    def path(self):
        return ctypes.string_at(self.pointer)

    def __repr__(self):
        return '{}.from_path({})'.format(
            self.__class__.__name__,
            repr(self.path),
        )
