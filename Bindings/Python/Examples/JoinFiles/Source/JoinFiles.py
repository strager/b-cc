#!/usr/bin/env python2.7

import _b

joined_paths = ['one.txt', 'two.txt', 'three.txt']

def main():
  with _b.Database.open_sqlite3(
    'JoinFiles.cache',
    _b.Database.SQLITE_OPEN_READWRITE
      | _b.Database.SQLITE_OPEN_CREATE,
  ) as database:
    database.check_all([_b.FileQuestion])
    run_loop = _b.RunLoopNative.preferred()
    def root_question_answered(future):
      return run_loop.stop()
    main = _b.Main(
      database=database,
      run_loop=run_loop,
      callback=dispatch_question,
    )
    answer_future = main.answer(
      _b.FileQuestion('joined.txt'),
    )
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
  if path == 'joined.txt':
    build_joined(ac)
  elif path in joined_paths:
    ac.succeed()
  else:
    ac.fail()

def build_joined(ac):
  def join_callback(answer_future):
    state = answer_future.state
    if state == _b.AnswerFuture.PENDING:
      raise ValueError('Future resolved in PENDING state')
    elif state == _b.AnswerFuture.FAILED:
      ac.fail()
    elif state == _b.AnswerFuture.RESOLVED:
      try:
        join_files(ac)
      except Exception as e:
        ac.fail(e)
  questions = [
    _b.FileQuestion(path)
    for path
    in joined_paths
  ]
  future = ac.need(questions)
  future.add_callback(join_callback)

def join_files(ac):
  with open('joined.txt', 'w') as joined:
    for path in joined_paths:
      with open(path, 'r') as part:
        while True:
          buffer = part.read(1024)
          if not buffer:
            break
          joined.write(buffer)
  ac.succeed()

if __name__ == '__main__':
  exit(main())
