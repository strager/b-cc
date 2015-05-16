#!/usr/bin/env python2.7

import _b
import signal
import unittest

class TestProcessExitStatus(unittest.TestCase):
  def test_code(self):
    s = _b.ProcessExitStatusCode(19)
    self.assertEqual(19, s.exit_code)
    self.assertEqual(_b.ProcessExitStatusCode(19), s)
    self.assertNotEqual(_b.ProcessExitStatusCode(20), s)

  def test_signal(self):
    s = _b.ProcessExitStatusSignal(signal.SIGTERM)
    self.assertEqual(signal.SIGTERM, s.signal_number)
    self.assertEqual(
      _b.ProcessExitStatusSignal(signal.SIGTERM),
      s,
    )
    self.assertNotEqual(
      _b.ProcessExitStatusSignal(signal.SIGSEGV),
      s,
    )

  def test_exception(self):
    s = _b.ProcessExitStatusException(0xC0000004)
    self.assertEqual(0xC0000004, s.exception_code)
    self.assertEqual(
      _b.ProcessExitStatusException(0xC0000004),
      s,
    )
    self.assertNotEqual(
      _b.ProcessExitStatusException(0xC0000002),
      s,
    )

  def test_mix(self):
    self.assertNotEqual(
      _b.ProcessExitStatusException(0),
      _b.ProcessExitStatusCode(0),
    )
    self.assertNotEqual(
      _b.ProcessExitStatusException(0),
      _b.ProcessExitStatusSignal(0),
    )
    self.assertNotEqual(
      _b.ProcessExitStatusCode(0),
      _b.ProcessExitStatusSignal(0),
    )

if __name__ == '__main__':
  unittest.main()
