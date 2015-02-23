from b.database import Database
from b.gen import gen_return
from b.main import Main
from b.test.mock import MockAnswer
from b.test.mock import MockQuestion
import unittest

class TestMain(unittest.TestCase):
    def test_allocate_deallocate(self):
        main = Main()
        # Do nothing.
        del main  # Fix lint.

    def test_callback_called_for_initial_question(self):
        database = Database.load_sqlite(
            ':memory:',
            flags=Database.SQLITE_OPEN_CREATE
                | Database.SQLITE_OPEN_READWRITE,
        )
        question = MockQuestion()
        answer = MockAnswer()

        main = Main()
        dispatch_calls = [0]
        def gen_dispatch_callback(answer_context):
            dispatch_calls[0] += 1
            yield answer_context.gen_success_answer(answer)
            yield gen_return(None)
        actual_answer = main.loop(
            question=question,
            database=database,
            gen_dispatch_callback=gen_dispatch_callback,
        )
        self.assertEqual(1, dispatch_calls[0])
        self.assertIs(answer, actual_answer)

if __name__ == '__main__':
    unittest.main()
