from b.question import Answer
from b.question import Question
from b.question import UUID
import hashlib

class FileAnswer(Answer):
    def __init__(self, hash_bytes):
        self.__hash_bytes = str(hash_bytes)

    @staticmethod
    def question_class():
        raise NotImplementedError()

    def serialize(self, byte_sink):
        byte_sink.write_data_and_size_8_be(
            self.__hash_bytes,
        )

    @classmethod
    def deserialize(cls, byte_source):
        hash_bytes = byte_source.read_data_and_size_8_be()
        return cls(hash_bytes)

    def __eq__(self, other):
        if type(self) is not type(other):
            return NotImplemented
        if type(self) is not FileAnswer:
            return NotImplemented
        return self.__hash_bytes == other.__hash_bytes

class FileQuestion(Question):
    def __init__(self, path):
        self.__path = str(path)

    @staticmethod
    def uuid():
        return UUID(
            0x4BEA5A85,
            0x699A,
            0x41AC,
            0xBEC7,
            0xEE2C347266CE,
        )

    @staticmethod
    def answer_class():
        return FileAnswer

    @classmethod
    def from_path(cls, path):
        return cls(path)

    @property
    def path(self):
        return self.__path

    def query_answer(self):
        buffer_size = 64 * 1024
        hasher = hashlib.sha256()
        with open(self.__path, 'rb') as file:
            while True:
                data = file.read(buffer_size)
                if len(data) == 0:
                    break
                hasher.update(data)
            return FileAnswer(hasher.digest())

    def serialize(self, byte_sink):
        byte_sink.write_data_and_size_8_be(self.__path)

    @classmethod
    def deserialize(cls, byte_source):
        path = byte_source.read_data_and_size_8_be()
        return cls.from_path(path)

    def __eq__(self, other):
        if type(self) is not type(other):
            return NotImplemented
        if type(self) is not FileQuestion:
            return NotImplemented
        return self.path == other.path

    def __repr__(self):
        return '{}.from_path({})'.format(
            self.__class__.__name__,
            repr(self.path),
        )
