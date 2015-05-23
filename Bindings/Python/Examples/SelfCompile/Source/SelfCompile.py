#!/usr/bin/env python2.7

import _b
import os.path
import pipes
import subprocess
import sys

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
    run_loop = _b.RunLoop.preferred()
    def root_question_answered(future):
      return run_loop.stop()
    main = _b.Main(
      database=database,
      run_loop=run_loop,
      callback=dispatch_question,
    )
    answer_future = main.answer(_b.FileQuestion(out_path))
    answer_future.add_callback(root_question_answered)
    run_loop.run()
    state = answer_future.state
    if state == _b.AnswerFuture.PENDING:
      print('Answer future still PENDING')
      return 1
    elif state == _b.AnswerFuture.FAILED:
      print('Answering question FAILED')
      return 2
    elif state == _b.AnswerFuture.RESOLVED:
      return 0
    else:
      raise ValueError('Unknown AnswerFuture state')

def dispatch_question(main, ac):
  path = ac.question.path
  print('dispatch_question({})'.format(path))
  _, extension = os.path.splitext(path)
  if path == out_path:
    link(ac)
  elif extension == '.o':
    compile(path, ac)
  elif extension == '.c':
    ac.succeed()
  else:
    ac.fail()

def compile(obj_path, ac):
  base, _ = os.path.splitext(obj_path)
  c_path = base + '.c'
  def callback(answer_future):
    state = answer_future.state
    if state == _b.AnswerFuture.PENDING:
      raise ValueError('Future resolved in PENDING state')
    elif state == _b.AnswerFuture.FAILED:
      ac.fail()
    elif state == _b.AnswerFuture.RESOLVED:
      try:
        execute(ac, [
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
      except Exception as e:
        ac.fail(e)
  future = ac.need([_b.FileQuestion(c_path)])
  future.add_callback(callback)

def link(ac):
  def callback(answer_future):
    state = answer_future.state
    if state == _b.AnswerFuture.PENDING:
      raise ValueError('Future resolved in PENDING state')
    elif state == _b.AnswerFuture.FAILED:
      ac.fail()
    elif state == _b.AnswerFuture.RESOLVED:
      try:
        execute(ac, [
          'cc',
          '-o', out_path,
        ] + objects + [
          '-ldl',  # For Linux.
          '-lpthread',  # For Linux.
        ])
      except Exception as e:
        ac.fail(e)
  future = ac.need([
    _b.FileQuestion(obj_path)
    for obj_path in objects
  ])
  future.add_callback(callback)

def execute(ac, command_args):
  print(' '.join(map(pipes.quote, command_args)))
  def callback(exit_status):
    if exit_status == _b.ProcessExitStatusCode(0):
      ac.succeed()
    else:
      ac.fail(exit_status)
  try:
    spawn_process(
      args=command_args,
      run_loop=ac.run_loop,
      callback=callback,
    )
  except Exception as e:
    print(e)
    raise

def spawn_process(args, run_loop, callback):
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

if __name__ == '__main__':
  exit(main())
