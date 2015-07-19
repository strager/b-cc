#!/usr/bin/env python2.7

from twisted.internet import reactor
from twisted.internet.defer import inlineCallbacks
from twisted.internet.defer import returnValue
import _b
import b.twisted
import os.path
import pipes
import twisted.internet.error
import twisted.internet.utils

out_path = 'SelfCompile'

sources = [
  'Examples/SelfCompile/Source/Main.c',
  'Source/AnswerContext.c',
  'Source/AnswerFuture.c',
  'Source/Assertions.c',
  'Source/Database.c',
  'Source/FileQuestion.c',
  'Source/Main.c',
  'Source/Memory.c',
  'Source/Mutex.c',
  'Source/QuestionAnswer.c',
  'Source/RunLoop.c',
  'Source/RunLoopKqueue.c',
  'Source/RunLoopSigchld.c',
  'Source/RunLoopUtil.c',
  'Source/SQLite3.c',
  'Source/Serialize.c',
  'Source/UUID.c',
  'Vendor/sphlib-3.0/c/sha2.c',
  'Vendor/sqlite-3.8.4.1/sqlite3.c',
]

objects = [
  os.path.splitext(c_path)[0] + '.o'
  for c_path in sources
]

def main():
  with _b.Database.open_sqlite3(
    'SelfCompile.cache',
    _b.Database.SQLITE_OPEN_READWRITE
      | _b.Database.SQLITE_OPEN_CREATE,
  ) as database:
    database.check_all([_b.FileQuestion])
    def root_question_answered(future):
      # TODO(strager): Remove the callWhenRunning.
      # AnswerFuture callbacks should always be called using
      # the RunLoop.
      reactor.callWhenRunning(lambda: reactor.crash())
    def root_question_error(e):
      reactor.crash()
      raise  # Not a typo. This will re-raise e.
    main = b.twisted.TwistedMain(
      database=database,
      callback=dispatch_question,
    )
    answer_deferred = main.answer(_b.FileQuestion(out_path))
    answer_deferred.addCallbacks(
      root_question_answered,
      root_question_error,
    )
    reactor.run()
    if not answer_deferred.called:
      print('Answer deferred is still uncalled')
      return 1
    # FIXME(strager): There has to be a better way...
    result = []
    answer_deferred.addCallbacks(
      callback=lambda _: result.append(0),
      errback=lambda _: result.append(2),
    )
    if len(result) != 1:
      raise ValueError(
        'Expected Answer deferred callback to be called '
        'exactly once',
      )
    if result[0] != 0:
      print('Answering question FAILED')
    return result[0]

@inlineCallbacks
def dispatch_question(ac):
  path = ac.question.path
  print('dispatch_question({})'.format(path))
  _, extension = os.path.splitext(path)
  if path == out_path:
    result = yield link(ac)
    returnValue(result)
  elif extension == '.o':
    result = yield compile(path, ac)
    returnValue(result)
  elif extension == '.c':
    returnValue(None)
  else:
    raise ValueError('Unknown path {}'.format(path))

@inlineCallbacks
def compile(obj_path, ac):
  base, _ = os.path.splitext(obj_path)
  c_path = base + '.c'
  yield ac.need([_b.FileQuestion(c_path)])
  yield execute([
      'cc',
      '-I', 'Headers',
      '-I', 'PrivateHeaders',
      '-I', 'Vendor/sphlib-3.0/c',
      '-I', 'Vendor/sqlite-3.8.4.1',
      '-std=c99',
      # See various CMakeLists.txt.
      '-D_POSIX_SOURCE',
      '-D_POSIX_C_SOURCE=200809L',
      '-D_DARWIN_C_SOURCE',
      '-o', obj_path,
      '-c',
      c_path,
    ])
  returnValue(None)

@inlineCallbacks
def link(ac):
  yield ac.need([
    _b.FileQuestion(obj_path)
    for obj_path in objects
  ])
  yield execute([
    'cc',
    '-o', out_path,
  ] + objects + [
    '-ldl',  # For Linux.
    '-lpthread',  # For Linux.
  ])
  returnValue(None)

@inlineCallbacks
def execute(command_args):
  print(' '.join(map(pipes.quote, command_args)))
  value = yield twisted.internet.utils.getProcessValue(
    executable=command_args[0],
    args=command_args[1:],
    env=os.environ,
  )
  if value == 0:
    returnValue(None)
  else:
    raise value

if __name__ == '__main__':
  exit(main())
