#!/usr/bin/env python2.7

import _b
import unittest

class TestFileQuestion(unittest.TestCase):
  def test_path(self):
    q = _b.FileQuestion('my_path')
    self.assertEqual('my_path', q.path)

if __name__ == '__main__':
  unittest.main()
