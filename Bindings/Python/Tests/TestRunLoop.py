#!/usr/bin/env python2.7

import _b
import select
import signal
import subprocess
import sys
import unittest

class TestRunLoop(unittest.TestCase):
  def test_allocate_preferred(self):
    run_loop = _b.RunLoop.preferred()
    _ = run_loop

class Callback(object):
  def __init__(
    self,
    list,
    stop_run_loop=None,
    stop_if=lambda: True,
  ):
    self.__list = list
    self.__run_loop = stop_run_loop
    self.__stop_if = stop_if

  def __call__(self, *args, **kwargs):
    self.__list.append((self, args, kwargs))
    if self.__run_loop is not None and self.__stop_if():
      self.__run_loop.stop()

class ProcessTree(object):
  def __init__(
    self,
    run_loop,
    execute,
    children=[],
    test=None,
  ):
    self.__run_loop = run_loop
    self.__execute = execute
    self.__children = list(children)
    self.__test = test
    self.__root = None
    self.__exited = False
    self.__exit_status = None
    self.__set_root(self)

  def execute(self):
    self.__execute(self.__exec_callback)

  @property
  def fully_exited(self):
    if self.__exit_status is None:
      return False
    return all(
      child.fully_exited
      for child in self.__children
    )

  def __set_root(self, root):
    self.__root = root
    for child in self.__children:
      child.__set_root(root)

  def __exec_callback(self, exit_status):
    assert exit_status is not None
    assert self.__exit_status is None
    self.__exit_status = exit_status
    try:
      if self.__test is not None:
        self.__test(self.__exit_status)
      for child in self.__children:
        child.execute()
    finally:
      if self.__root.fully_exited:
        self.__run_loop.stop()

class TestRunLoopMixin(object):
  def _create_run_loop(self):
    raise NotImplementedError(
      'Implement _run_loop in a subclass',
    )

  def test_stop_function(self):
    rl = self._create_run_loop()
    called = []
    callback = Callback(called, stop_run_loop=rl)
    rl.add_function(callback)
    self.assertEqual([], called)
    rl.run()
    self.assertEqual([(callback, (), {})], called)

  def test_stop_function(self):
    rl = self._create_run_loop()
    called = []
    callback_1 = Callback(called, stop_run_loop=rl)
    callback_2 = Callback(called, stop_run_loop=rl)
    rl.add_function(callback_1)
    rl.add_function(callback_2)
    self.assertEqual([], called)
    rl.run()
    self.assertTrue(
      (callback_1, (), {}) in called
        or (callback_2, (), {}) in called,
      'Callback should have been called',
    )
    self.assertNotEqual(
      (callback_1, (), {}) in called,
      (callback_2, (), {}) in called,
      'Only one callback should have been called'
    )

  def test_true_process(self):
    rl = self._create_run_loop()
    called = []
    callback = Callback(called, stop_run_loop=rl)
    self._spawn_process(
      args=['true'],
      run_loop=rl,
      callback=callback,
    )
    rl.run()
    self.assertEqual(
      [(callback, (_b.ProcessExitStatusCode(0),), {})],
      called,
    )

  def test_false_process(self):
    rl = self._create_run_loop()
    called = []
    callback = Callback(called, stop_run_loop=rl)
    self._spawn_process(
      args=['false'],
      run_loop=rl,
      callback=callback,
    )
    rl.run()
    self.assertEqual(
      [(callback, (_b.ProcessExitStatusCode(1),), {})],
      called,
    )

  def test_kill_sigterm(self):
    rl = self._create_run_loop()
    called = []
    callback = Callback(called, stop_run_loop=rl)
    self._spawn_process(
      args=['sh', '-c', 'kill -TERM $$'],
      run_loop=rl,
      callback=callback,
    )
    rl.run()
    self.assertEqual([(
      callback,
      (_b.ProcessExitStatusSignal(signal.SIGTERM),),
      {},
    )], called)

  def test_multiple_trues_in_parallel(self):
    rl = self._create_run_loop()
    called = []
    callbacks = [Callback(
      called,
      stop_run_loop=rl,
      stop_if=lambda: len(called) >= len(callbacks),
    ) for _ in range(20)]
    for callback in callbacks:
      self._spawn_process(
        args=['true'],
        run_loop=rl,
        callback=callback,
      )
    rl.run()
    self.maxDiff = None
    self.assertItemsEqual(
      [
        (callback, (_b.ProcessExitStatusCode(0),), {})
        for callback in callbacks
      ],
      called,
    )

  def test_deeply_nested_process_tree(self):
    rl = self._create_run_loop()
    def execute(callback):
      self._spawn_process(['true'], rl, callback)
    def test(status):
      self.assertEqual(_b.ProcessExitStatusCode(0), status)
    process_tree = ProcessTree(rl, execute, test=test)
    for _ in range(50):
      process_tree = ProcessTree(
        rl,
        execute,
        children=[process_tree],
        test=test,
      )
    process_tree.execute()
    rl.run()
    self.assertTrue(process_tree.fully_exited)

  def test_very_branchy_process_tree(self):
    rl = self._create_run_loop()
    def execute(callback):
      self._spawn_process(['true'], rl, callback)
    def test(status):
      self.assertEqual(_b.ProcessExitStatusCode(0), status)
    tree = ProcessTree(rl, execute, test=test, children=[
      ProcessTree(rl, execute, test=test, children=[
        ProcessTree(rl, execute, test=test)
        for _ in range(10)
      ])
      for _ in range(10)
    ])
    tree.execute()
    rl.run()
    self.assertTrue(tree.fully_exited)

  def _spawn_process(self, args, run_loop, callback):
    if sys.platform == 'win32':
      raise NotImplementedError()
    else:
      process = subprocess.Popen(args)
      def real_callback(exit_status):
        # HACK(strager): Prevent Popen from cleaning up
        # after itself.
        process.returncode = 0
        return callback(exit_status)
      run_loop.add_process_id(process.pid, real_callback)

class TestRunLoopPreferred(
  unittest.TestCase,
  TestRunLoopMixin,
):
  def _create_run_loop(self):
    return _b.RunLoop.preferred()

@unittest.skipUnless(
  hasattr(select, 'kqueue'),
  'need select.kqueue()',
)
class TestRunLoopKqueue(
  unittest.TestCase,
  TestRunLoopMixin,
):
  def _create_run_loop(self):
    return _b.RunLoop.kqueue()

class TestRunLoopSigchld(
  unittest.TestCase,
  TestRunLoopMixin,
):
  def _create_run_loop(self):
    return _b.RunLoop.sigchld()

if __name__ == '__main__':
  unittest.main()
