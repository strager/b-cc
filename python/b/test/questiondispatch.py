from b.database import Database
from b.questiondispatch import question_dispatch_one
from b.questionqueue import QuestionQueue
from b.test.mock import MockAnswer
from b.test.mock import MockQuestion
from b.test.mock import MockQuestionQueueItem
import b.gen as gen
import unittest

class TestQuestionDispatch(unittest.TestCase):
    def test_empty_database_success(self):
        self.longMessage = True

        question = MockQuestion()
        queue_item = MockQuestionQueueItem(question)

        queue = QuestionQueue.single_threaded()

        database = Database.load_sqlite(
            ':memory:',
            flags=Database.SQLITE_OPEN_CREATE
                | Database.SQLITE_OPEN_READWRITE,
        )

        answer_context = question_dispatch_one(
            queue_item=queue_item,
            queue=queue,
            database=database,
        )

        answer = MockAnswer()
        with queue_item.expect_on_answer(self, answer):
            gen.loop(
                answer_context.gen_success_answer(answer),
            )

    def test_fail_callback_called_immediately(self):
        question = MockQuestion()
        queue_item = MockQuestionQueueItem(question)

        queue = QuestionQueue.single_threaded()

        database = Database.load_sqlite(
            ':memory:',
            flags=Database.SQLITE_OPEN_CREATE
                | Database.SQLITE_OPEN_READWRITE,
        )

        answer_context = question_dispatch_one(
            queue_item=queue_item,
            queue=queue,
            database=database,
        )

        with queue_item.expect_on_answer(self, None):
            gen.loop(answer_context.gen_error())

if __name__ == '__main__':
    unittest.main()
