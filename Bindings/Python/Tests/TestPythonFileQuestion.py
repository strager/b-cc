#!/usr/bin/env python2.7

import _b
import binascii
import hashlib
import tempfile
import unittest

class FileQuestion(_b.Question):
  def __init__(self, path):
    self.__path = str(path)

  @property
  def path(self):
    return self.__path

  def query_answer(self):
    BUFFER_SIZE = 4096
    hasher = hashlib.sha256()
    with open(self.path, 'rb') as f:
      while True:
        bytes = f.read(BUFFER_SIZE)
        if not bytes:
          break
        hasher.update(bytes)
    return FileAnswer(hasher.digest())

  def serialize(self, sink):
    sink.serialize_data_and_size_8_be(self.path)

  @classmethod
  def deserialize(cls, source):
    length = source.deserialize_8_be()
    path = source.deserialize_bytes(length)
    return cls(str(path))

class FileAnswer(_b.Answer):
  def __init__(self, digest_bytes):
    if len(digest_bytes) != 32:
      raise ValueError('Invalid digest')
    self.__digest = str(digest_bytes)

  @property
  def digest(self):
    return self.__digest

  def serialize(self, sink):
    sink.serialize_data(self.__digest)

  @classmethod
  def deserialize(cls, source):
    digest = source.deserialize_bytes(32)
    return cls(digest)

class TestFileQuestion(unittest.TestCase):
  def setUp(self):
    self.run_loop = _b.RunLoopNative.preferred()
    self.database = _b.Database.open_sqlite3(
      ':memory:',
      _b.Database.SQLITE_OPEN_READWRITE,
    )

  def tearDown(self):
    self.database.close()

  def test_path(self):
    q = FileQuestion('my_path')
    self.assertEqual('my_path', q.path)

  def test_main_dispatch_same_question(self):
    q = FileQuestion('my_path')
    rl = self.run_loop
    def dispatch_question(main, ac):
      self.assertSame(q, ac.question)
      ac.fail()
      rl.stop()
    main = _b.Main(
      database=self.database,
      run_loop=rl,
      callback=dispatch_question,
    )
    main.answer(q)
    rl.run()

  def test_main_dispatch_query_answer(self):
    with tempfile.NamedTemporaryFile() as file:
      q = FileQuestion(file.name)
      rl = self.run_loop
      def dispatch_question(main, ac):
        ac.succeed()
        rl.stop()
      main = _b.Main(
        database=self.database,
        run_loop=rl,
        callback=dispatch_question,
      )
      future = main.answer(q)
      rl.run()
      self.assertEqual(1, len(future.answers))
      self.assertIsInstance(future.answers[0], FileAnswer)
      self.assertEqual(
        'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca4'
          '95991b7852b855',
        binascii.hexlify(future.answers[0].digest),
      )

  def test_main_dispatch_given_answer(self):
    q = FileQuestion('my_path')
    a = FileAnswer(binascii.unhexlify(
      '2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e7304'
        '3362938b9824',
    ))
    rl = self.run_loop
    def dispatch_question(main, ac):
      ac.succeed_answer(a)
      rl.stop()
    main = _b.Main(
      database=self.database,
      run_loop=rl,
      callback=dispatch_question,
    )
    future = main.answer(q)
    rl.run()
    self.assertEqual(1, len(future.answers))
    self.assertSame(a, future.answers[0])

if __name__ == '__main__':
  unittest.main()
