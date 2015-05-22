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
    self.assertEqual(
      'ProcessExitStatusCode(19)',
      repr(_b.ProcessExitStatusCode(19)),
    )

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
    self.assertEqual(
      'ProcessExitStatusSignal(signal.SIGSEGV)',
      repr(_b.ProcessExitStatusSignal(signal.SIGSEGV)),
    )
    self.assertEqual(
      'ProcessExitStatusSignal(0)',
      repr(_b.ProcessExitStatusSignal(0)),
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
    self.assertEqual(
      'ProcessExitStatusException(0xC0000004)',
      repr(_b.ProcessExitStatusException(0xC0000004)),
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
