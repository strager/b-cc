from b.question import Question
from b.question import QuestionBase
from b.uuid import UUID
import unittest

class NotAQuestion(object):
    pass

class IncompleteQuestionBase(QuestionBase):
    # Do not define all methods.
    pass

class CompleteQuestionBase(QuestionBase):
    @staticmethod
    def answer_class():
        raise NotImplementedError()

    @staticmethod
    def vtable_pointer():
        raise NotImplementedError()

class CompleteQuestionBaseSubclass(CompleteQuestionBase):
    pass

class IncompleteQuestion(Question):
    # Do not define all methods.
    pass

class CompleteQuestion(Question):
    @staticmethod
    def uuid():
        return UUID(
            0x8C63DBD4,
            0x05A8,
            0x4B0C,
            0xA0AE,
            0x66FA357531D4,
        )

    @staticmethod
    def answer_class():
        raise NotImplementedError()

    def query_answer(self):
        raise NotImplementedError()

    def __eq__(self, other):
        raise NotImplementedError()

    def serialize(self, byte_sink):
        raise NotImplementedError()

    @staticmethod
    def deserialize(byte_source):
        raise NotImplementedError()

class CompleteQuestionSubclass(CompleteQuestion):
    pass

class TestQuestion(unittest.TestCase):
    def test_all_classes(self):
        classes = list(QuestionBase.yield_all_classes())
        self.assertFalse(
            NotAQuestion in classes,
            'NotAQuestion',
        )
        self.assertFalse(
            IncompleteQuestionBase in classes,
            'IncompleteQuestionBase',
        )
        self.assertFalse(
            CompleteQuestionBase in classes,
            'CompleteQuestionBase',
        )
        self.assertFalse(
            CompleteQuestionBaseSubclass in classes,
            'CompleteQuestionBaseSubclass',
        )
        self.assertFalse(
            IncompleteQuestion in classes,
            'IncompleteQuestion',
        )
        self.assertTrue(
            CompleteQuestion in classes,
            'CompleteQuestion',
        )
        self.assertTrue(
            CompleteQuestionSubclass in classes,
            'CompleteQuestionSubclass',
        )

if __name__ == '__main__':
    unittest.main()
