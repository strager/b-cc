#!/usr/bin/env python2.7

import _b
import unittest

class TestRunLoop(unittest.TestCase):
  def test_allocate_preferred(self):
    run_loop = _b.RunLoop.preferred()
    _ = run_loop

if __name__ == '__main__':
  unittest.main()
