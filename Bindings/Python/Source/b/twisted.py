from __future__ import absolute_import

#import _b
import twisted.internet.error
from twisted.internet.error import AlreadyCalled
from twisted.internet.error import AlreadyCancelled
from twisted.internet.interfaces import IReactorCore
from twisted.internet.interfaces import IReactorTime

#class TwistedRunLoop(_b.RunLoop):
class TwistedRunLoop(object):
  @classmethod
  def with_global_reactor(cls):
    from twisted.internet import reactor
    return cls(reactor)

  def __init__(self, reactor):
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
