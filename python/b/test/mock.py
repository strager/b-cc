from b.question import Answer
from b.question import Question
from b.questionqueue import QuestionQueueItem
from b.uuid import UUID
import contextlib

class MockQuestion(Question):
    @staticmethod
    def uuid():
        return UUID(
            0x2FEEB032,
            0x3DE9,
            0x455B,
            0xAD09,
            0x5A218A80009F,
        )

    @staticmethod
    def answer_class():
        return MockAnswer

    def __eq__(self, other):
        raise NotImplementedError()

    def query_answer(self):
        raise NotImplementedError()

    def serialize(self, byte_sink):
        return NotImplementedError()

    @classmethod
    def deserialize(cls, byte_source):
        raise NotImplementedError()

class MockAnswer(Answer):
    @staticmethod
    def question_class():
        return MockQuestion

    def __eq__(self, other):
        raise NotImplementedError()

    def serialize(self, byte_sink):
        return NotImplementedError()

    @classmethod
    def deserialize(cls, byte_source):
        raise NotImplementedError()

class MockQuestionQueueItem(QuestionQueueItem):
    def __init__(self, question):
        super(MockQuestionQueueItem, self) \
            .__init__(question)
        self.__on_answer = None

    def on_answer(self, answer):
        if self.__on_answer is None:
            raise Exception('Unexpected call to on_answer')
        self.__on_answer(answer)

    @contextlib.contextmanager
    def expect_on_answer(self, test_case, expected_answer):
        if self.__on_answer is not None:
            raise Exception('Already expecting answer')

        received_calls = []
        def on_answer(answer):
            received_calls.append(answer)
            if len(received_calls) > 1:
                raise Exception(
                    'on_answer called more than once',
                )
        self.__on_answer = on_answer
        try:
            yield
        finally:
            self.__on_answer = None
            test_case.assertEqual(
                1,
                len(received_calls),
                'on_answer should have been called exactly '
                'once',
            )
            test_case.assertIs(
                expected_answer,
                received_calls[0],
                'on_answer should have been called with '
                'the given answer',
            )
