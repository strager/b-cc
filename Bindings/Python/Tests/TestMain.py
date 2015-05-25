#!/usr/bin/env python2.7

import _b
import unittest

class TestRunLoop(unittest.TestCase):
  def setUp(self):
    self.run_loop = _b.RunLoopNative.preferred()
    self.database = _b.Database.open_sqlite3(
      ':memory:',
      _b.Database.SQLITE_OPEN_READWRITE,
    )

  def tearDown(self):
    self.database.close()

  def test_allocate(self):
    def dispatch_question(main, ac):
      self.fail()
    main = _b.Main(
      database=self.database,
      run_loop=self.run_loop,
      callback=dispatch_question,
    )
    _ = main

if __name__ == '__main__':
  unittest.main()
