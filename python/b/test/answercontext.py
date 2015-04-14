from b.questionqueue import QuestionQueue
from b.answercontext import AnswerContext
from b.test.mock import MockAnswer
from b.test.mock import MockQuestion
import b.gen as gen
import unittest

class TestAnswerContext(unittest.TestCase):
    def test_allocate_deallocate(self):
        question = MockQuestion()
        needed_question = MockQuestion()

        question_queue = QuestionQueue.single_threaded()

        (_answer_context, _answer_future) \
            = AnswerContext.create(
                question=question,
                question_queue=question_queue,
                database=None,
                process_controller=None,
            )

    def test_need_one_enqueues(self):
        '''
        Ensures b_answer_context_need_one enqueues the
        question on the QuestionQueue exactly once.
        '''

        question = MockQuestion()
        needed_question = MockQuestion()

        question_queue = QuestionQueue.single_threaded()

        (answer_context, _answer_future) \
            = AnswerContext.create(
                question=question,
                question_queue=question_queue,
                database=None,
                process_controller=None,
            )

        import sys
        print('Before: {}'.format(sys.getrefcount(needed_question)))
        future = answer_context.gen_need([needed_question])
        gen.loop(future)
        print('After: {}'.format(sys.getrefcount(needed_question)))
        return

        (first_queue_item, closed) \
            = question_queue.try_dequeue()
        self.assertIsNotNone(first_queue_item)
        self.assertFalse(closed)

        self.assertIs(
            needed_question,
            first_queue_item.question,
        )

        del first_queue_item

        (second_queue_item, closed) \
            = question_queue.try_dequeue()
        self.assertIsNone(second_queue_item)
        self.assertFalse(closed)

    def test_answer_success_calls_need_callback(self):
        '''
        Ensures calling gen_need's enqueued item's on_answer
        with an answer calls the success callback.
        '''
        question = MockQuestion()
        needed_question = MockQuestion()
        answer = MockAnswer()

        question_queue = QuestionQueue.single_threaded()

        (answer_context, _answer_future) \
            = AnswerContext.create(
                question=question,
                question_queue=question_queue,
                database=None,
                process_controller=None,
            )

        need_callback_answers = []
        future = answer_context.gen_need([needed_question])
        def on_answers(answers):
            need_callback_answers.append(answers)
        gen.loop(future, on_answers)

        (queue_item, closed) \
            = question_queue.try_dequeue()
        self.assertIsNotNone(queue_item)
        self.assertFalse(closed)

        self.assertIs(needed_question, queue_item.question)
        self.assertEqual([], need_callback_answers)

        queue_item.on_answer(answer)
        self.assertEqual(1, len(need_callback_answers))
        self.assertEqual(1, len(need_callback_answers[0]))
        self.assertIs(answer, need_callback_answers[0][0])

    def test_answer_success_success_calls_need_callbacks(
        self,
    ):
        '''
        Ensures calling the enqueued item on_answer-s of two
        gen_need calls with an answer calls the gen_need
        success callback.
        '''
        question = MockQuestion()
        needed_question_1 = MockQuestion();
        needed_question_2 = MockQuestion()
        answer_1 = MockAnswer()
        answer_2 = MockAnswer()

        question_queue = QuestionQueue.single_threaded()

        (answer_context, _answer_future) \
            = AnswerContext.create(
                question=question,
                question_queue=question_queue,
                database=None,
                process_controller=None,
            )

        need_callback_1_answers = []
        future_1 = answer_context.gen_need([
            needed_question_1,
        ])
        def on_answers_1(answers):
            need_callback_1_answers.append(answers)
        gen.loop(future_1, on_answers_1)

        need_callback_2_answers = []
        future_2 = answer_context.gen_need([
            needed_question_2,
        ])
        def on_answers_2(answers):
            need_callback_2_answers.append(answers)
        gen.loop(future_2, on_answers_2)

        # Pop one.
        (queue_item_1, closed) \
            = question_queue.try_dequeue()
        self.assertIsNotNone(queue_item_1)
        self.assertFalse(closed)

        if queue_item_1.question is needed_question_1:
            queue_item_1_answer = answer_1
            queue_item_1_need_callback_answers \
                = need_callback_1_answers
        elif queue_item_1.question is needed_question_2:
            queue_item_1_answer = answer_2
            queue_item_1_need_callback_answers \
                = need_callback_2_answers
        else:
            self.fail()
        self.assertEqual(
            0,
            len(queue_item_1_need_callback_answers),
        )
        queue_item_1.on_answer(queue_item_1_answer)
        self.assertEqual(
            1,
            len(queue_item_1_need_callback_answers),
        )

        # Pop another.
        (queue_item_2, closed) \
            = question_queue.try_dequeue()
        self.assertIsNotNone(queue_item_2)
        self.assertFalse(closed)

        if queue_item_2.question is needed_question_1:
            queue_item_2_answer = answer_1
            queue_item_2_need_callback_answers \
                = need_callback_1_answers
        elif queue_item_2.question is needed_question_2:
            queue_item_2_answer = answer_2
            queue_item_2_need_callback_answers \
                = need_callback_2_answers
        else:
            self.fail()
        self.assertEqual(
            0,
            len(queue_item_2_need_callback_answers),
        )
        queue_item_2.on_answer(queue_item_2_answer)
        self.assertEqual(
            1,
            len(queue_item_2_need_callback_answers),
        )

    def test_answer_error_calls_need_callback(self):
        '''
        Ensures calling gen_need's enqueued item on_answer
        without an answer calls the gen_need's error
        callback.
        '''

        question = MockQuestion()
        needed_question = MockQuestion()

        question_queue = QuestionQueue.single_threaded()

        (answer_context, _answer_future) \
            = AnswerContext.create(
                question=question,
                question_queue=question_queue,
                database=None,
                process_controller=None,
            )

        need_callback_answers = []
        future = answer_context.gen_need([needed_question])
        def on_answers(answers):
            need_callback_answers.append(answers)
        gen.loop(future, on_answers)

        (queue_item, closed) \
            = question_queue.try_dequeue()
        self.assertIsNotNone(queue_item)
        self.assertFalse(closed)

        self.assertIs(
            needed_question,
            queue_item.question,
        )
        self.assertEqual([], need_callback_answers)

        queue_item.on_answer(None)
        self.assertEqual([None], need_callback_answers)

    def test_success_answer_calls_context_callback(self):
        '''
        Ensures calling gen_success_answer calls the
        AnswerContext's answer_callback with the answer.
        '''

        question = MockQuestion()
        answer = MockAnswer()

        question_queue = QuestionQueue.single_threaded()

        (answer_context, answer_future) \
            = AnswerContext.create(
                question=question,
                question_queue=question_queue,
                database=None,
                process_controller=None,
            )

        gen.loop(answer_context.gen_success_answer(answer))
        self.assertTrue(answer_future.is_resolved)
        self.assertIs(
            answer,
            answer_future.get_resolved_value(),
        )

if __name__ == '__main__':
    unittest.main()
