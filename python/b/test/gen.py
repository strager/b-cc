import unittest

from b.gen import Future
from b.gen import gen_return
from b.gen import loop

class _MockCallback(object):
    def __init__(self):
        self.__called = False
        self.__kargs = None
        self.__kwargs = None

    def call(self, *kargs, **kwargs):
        self.__called = True
        self.__kargs = kargs
        self.__kwargs = kwargs

    @property
    def called(self):
        return self.__called

    @property
    def kargs(self):
        if not self.called:
            raise Exception()
        return self.__kargs

    @property
    def kwargs(self):
        if not self.called:
            raise Exception()
        return self.__kwargs

class TestGen(unittest.TestCase):
    def assertCalled(
            self,
            mock_callback,
            *kargs,
            **kwargs):
        self.assertTrue(mock_callback.called)
        self.assertEqual(kargs, mock_callback.kargs)
        self.assertEqual(kwargs, mock_callback.kwargs)

    def assertNotCalled(self, mock_callback):
        self.assertFalse(mock_callback.called)

    def test_empty_generator(self):
        def gen_nothing():
            return
            # Trick Python into making this function a
            # generator.
            yield
        callback = _MockCallback()
        loop(gen_nothing(), done_callback=callback.call)
        self.assertCalled(callback, None)

    def test_gen_return(self):
        def gen_message():
            yield gen_return('hello world')
        callback = _MockCallback()
        loop(gen_message(), done_callback=callback.call)
        self.assertCalled(callback, 'hello world')

    def test_gen_yield(self):
        def gen_outer():
            def gen_inner():
                def gen_inner_inner():
                    yield gen_return('a')
                value = yield gen_inner_inner()
                yield gen_return(value + 'b')
            value = yield gen_inner()
            yield gen_return(value + 'c')
        callback = _MockCallback()
        loop(gen_outer(), done_callback=callback.call)
        self.assertCalled(callback, 'abc')

    def test_gen_nested(self):
        def gen_nested():
            def gen_a():
                yield gen_return('a')
            def gen_b():
                yield gen_return('b')
            a = yield gen_a()
            b = yield gen_b()
            self.assertEqual('a', a)
            self.assertEqual('b', b)
            yield gen_return(('returned', a, b))
        callback = _MockCallback()
        loop(gen_nested(), done_callback=callback.call)
        self.assertCalled(callback, ('returned', 'a', 'b'))

    def test_resolved_future(self):
        future = Future()
        future.resolve('message')
        def gen_with_future():
            value = yield future
            self.assertEqual('message', value)
            yield gen_return(('returned', value))
        callback = _MockCallback()
        loop(gen_with_future(), done_callback=callback.call)
        self.assertCalled(callback, ('returned', 'message'))

    def test_unresolved_future(self):
        future = Future()
        def gen_with_future():
            value = yield future
            self.assertEqual('message', value)
            yield gen_return(('returned', value))
        callback = _MockCallback()
        loop(gen_with_future(), done_callback=callback.call)
        self.assertNotCalled(callback)
        future.resolve('message')
        self.assertCalled(callback, ('returned', 'message'))

    def test_cancel_unresolved_future_then_yield(self):
        future = Future()
        future.cancel()
        exception_callback = _MockCallback()
        def gen_with_future():
            try:
                yield future
                self.fail('future returned')
            except GeneratorExit, e:
                exception_callback.call(e)
            except:
                self.fail('Exception received')
        callback = _MockCallback()
        loop(gen_with_future())
        self.assertNotCalled(callback)
        self.assertTrue(exception_callback.called)
        self.assertIsInstance(
            exception_callback.kargs[0], GeneratorExit)

    def test_yield_then_cancel_future(self):
        future = Future()
        exception_callback = _MockCallback()
        def gen_with_future():
            try:
                yield future
                self.fail('future returned')
            except GeneratorExit, e:
                exception_callback.call(e)
            except:
                self.fail('Exception received')
        callback = _MockCallback()
        loop(gen_with_future(), done_callback=callback.call)
        self.assertNotCalled(callback)
        future.cancel()
        self.assertNotCalled(callback)
        self.assertTrue(exception_callback.called)
        self.assertIsInstance(
            exception_callback.kargs[0], GeneratorExit)

if __name__ == '__main__':
    unittest.main()
