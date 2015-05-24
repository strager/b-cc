from __future__ import absolute_import

from twisted.internet.defer import Deferred
from twisted.internet.error import AlreadyCalled
from twisted.internet.error import AlreadyCancelled
from twisted.internet.interfaces import IReactorCore
from twisted.internet.interfaces import IReactorTime
import _b
import twisted.internet.error

class TwistedRunLoop(_b.RunLoop):
  @classmethod
  def with_global_reactor(cls):
    from twisted.internet import reactor
    return cls(reactor)

  def __init__(self, reactor):
    super(TwistedRunLoop, self).__init__()
    if not IReactorCore.providedBy(reactor):
      raise TypeError('reactor must implement IReactorCore')
    if not IReactorTime.providedBy(reactor):
      raise TypeError('reactor must implement IReactorTime')
    self.__reactor = reactor
    # TODO(strager): Make thread-safe.
    self.__delayed_calls = []

  @property
  def reactor(self):
    return self.__reactor

  def add_function(self, function):
    self.__clean_delayed_calls()
    self.__delayed_calls.append(
      self.__reactor.callLater(0, function),
    )

  def run(self):
    self.__reactor.run()

  def stop(self):
    self.__reactor.crash()

  def __clean_delayed_calls(self):
    calls = self.__delayed_calls
    i = len(calls) - 1
    while i >= 0:
      if not calls[i].active():
        del calls[i]
      i = i - 1

#  def __del__(self):
#    self.__clean_delayed_calls()
#    # NOTE(strager): This can happen on any thread, hence
#    # the try-catch.
#    calls = self.__delayed_calls
#    while calls:
#      try:
#        calls.pop().cancel()
#      except AlreadyCalled, AlreadyCancelled:
#        pass

def deferred_from_answer_future(answer_future):
  deferred = Deferred()
  def callback(future):
    state = future.state
    if state == _b.AnswerFuture.PENDING:
      raise ValueError('Future resolved in PENDING state')
    elif state == _b.AnswerFuture.FAILED:
      deferred.errback(future)
    elif state == _b.AnswerFuture.RESOLVED:
      deferred.callback(future)
  answer_future.add_callback(callback)
  return deferred

class TwistedAnswerContext(object):
  def __init__(self, ac):
    self.__ac = ac

  @property
  def question(self):
    return self.__ac.question

  def need(self, questions):
    return deferred_from_answer_future(
      self.__ac.need(questions),
    )

class TwistedMain(object):
  def __init__(self, database, callback, reactor=None):
    if reactor is None:
      from twisted.internet import reactor
    self.__reactor = reactor
    self.__callback = callback
    self.__main = _b.Main(
      database=database,
      callback=self.__main_callback,
      run_loop=TwistedRunLoop(reactor),
    )

  def __main_callback(self, _main, ac):
    deferred = self.__callback(TwistedAnswerContext(ac))
    deferred.addCallbacks(
      callback=lambda _: ac.succeed(),
      errback=lambda e: ac.fail(e),
    )

  def answer(self, question):
    return deferred_from_answer_future(
      self.__main.answer(question),
    )
