'''
Tests for QuestionQueue, ported from TestQuestionQueue.cc.

TODO(strager): Implement eventfd tester for Linux.
'''

from b.long import long
from b.questionqueue import QuestionQueue
from b.test.mock import MockQuestion
from b.test.mock import MockQuestionQueueItem
from contextlib import contextmanager
import b.abc as abc
import select
import unittest

class TestQuestionQueueBase(object):
    @contextmanager
    def tester(self):
        tester = self._create_tester()
        yield tester
        tester.deallocate()

    def test_allocate_deallocate(self):
        with self.tester() as tester:
            queue = tester.create_queue()
            # Do nothing.
            del queue  # Fix lint.

    def test_enqueue_one_item_signals(self):
        with self.tester() as tester:
            question = MockQuestion()
            queue_item = MockQuestionQueueItem(question)
            queue = tester.create_queue()

            queue.enqueue(queue_item)
            self.assertTrue(tester.try_consume_event())

    def test_empty_queue_is_unsignaled(self):
        with self.tester() as tester:
            queue = tester.create_queue()
            if not tester.consumes_spurious_events:
                self.assertFalse(tester.try_consume_event())
            del queue  # Fix lint.

class Tester(abc.ABC):
    @abc.abstractmethod
    def create_queue(self):
        pass

    @property
    @abc.abstractmethod
    def consumes_spurious_events(self):
        '''
        Returns true if try_consume_event can have false
        positives.
        '''
        pass

    @abc.abstractmethod
    def try_consume_event(self):
        '''
        Tests if an event was given by the QuestionQueue.
        If so, the event is consumed.
        
        May have false positives iff
        consumes_spurious_events returns true.
        '''
        pass

    @abc.abstractmethod
    def deallocate(self):
        pass

class SingleThreadedTester(Tester):
    def create_queue(self):
        return QuestionQueue.single_threaded()

    @property
    def consumes_spurious_events(self):
        return True

    def try_consume_event(self):
        return True

    def deallocate(self):
        pass

class TestSingleThreadedQuestionQueue(
    unittest.TestCase,
    TestQuestionQueueBase,
):
    def _create_tester(self):
        return SingleThreadedTester()

# Not defined in the select module.
_KQ_FILTER_USER = -0xA
_KQ_NOTE_TRIGGER = 0x01000000

class KqueueTester(Tester):
    def __init__(self):
        self.__kqueue = select.kqueue()
        self.__kqueue.control([select.kevent(
            ident=self.__ident,
            filter=_KQ_FILTER_USER,
            flags=select.KQ_EV_ADD,
            fflags=_KQ_NOTE_TRIGGER,
        )], 0)

    @property
    def __ident(self):
        return long(id(self))

    def create_queue(self):
        return QuestionQueue.with_kqueue(
            kqueue=self.__kqueue,
            trigger_kevent=select.kevent(
                ident=self.__ident,
                filter=_KQ_FILTER_USER,
                flags=select.KQ_EV_ENABLE,
                fflags=_KQ_NOTE_TRIGGER,
            ),
        )

    @property
    def consumes_spurious_events(self):
        return False

    def try_consume_event(self):
        events = self.__kqueue.control(
            None,  # changelist
            2,  # max_events
            0,  # timeout
        )
        if not events:
            return False
        if events[0].ident != self.__ident:
            raise Exception(
                'Got event for different kqueue '
                '(expected {:#018x}, got {:#018x})'.format(
                    self.__ident,
                    events[0].ident,
                ),
            )
        if len(events) > 1:
            raise Exception('Too many events')
        return True

    def deallocate(self):
        self.__kqueue.close()

@unittest.skipUnless(
    hasattr(select, 'kqueue'),
    'need select.kqueue()'
)
class TestKqueueQuestionQueue(
    unittest.TestCase,
    TestQuestionQueueBase,
):
    def _create_tester(self):
        return KqueueTester()

if __name__ == '__main__':
    unittest.main()
