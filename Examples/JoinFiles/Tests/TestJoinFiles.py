#!/usr/bin/env python2.7

import contextlib
import os
import os.path
import pdb
import pipes
import shutil
import subprocess
import tempfile
import unittest

@contextlib.contextmanager
def temp_dir():
  dir = tempfile.mkdtemp()
  try:
    yield dir
  finally:
    shutil.rmtree(dir)

j = os.path.join

class TestJoinFiles(unittest.TestCase):
  def setUp(self):
    exe = os.getenv('JOIN_FILES_EXE')
    if exe is None:
      raise Exception(
        'Define the JOIN_FILES_EXE environment variable to '
        'be the path of your executable',
      )
    self.exe = os.path.realpath(exe)

  def execute(self, temp_dir, extra_args=[]):
    args = [self.exe] + extra_args
    if os.getenv('B_DEBUG_TEST'):
      print('Would execute: {}'.format(
        ' '.join(map(pipes.quote, args)),
      ))
      print('In directory : {}'.format(temp_dir))
      pdb.set_trace()
    return subprocess.call(args, cwd=temp_dir)


  def test_full_build(self):
    with temp_dir() as d:
      with open(j(d, 'one.txt'), 'wb') as one:
        one.write('hello world\n')
      with open(j(d, 'two.txt'), 'wb') as two:
        two.write('line two ')
      with open(j(d, 'three.txt'), 'wb') as three:
        three.write('and three\n')
      exit_code = self.execute(d)
      self.assertEqual(0, exit_code)
      self.assertTrue(os.path.exists(j(d, 'joined.txt')))
      with open(j(d, 'joined.txt'), 'rb') as joined:
        joined_contents = joined.read()
      self.assertEqual(
        'hello world\nline two and three\n',
        joined_contents,
      )

  def test_missing_all_inputs(self):
    with temp_dir() as d:
      exit_code = self.execute(d)
      self.assertEqual(1, exit_code)
      self.assertFalse(os.path.exists(j(d, 'one.txt')))
      self.assertFalse(os.path.exists(j(d, 'two.txt')))
      self.assertFalse(os.path.exists(j(d, 'three.txt')))
      self.assertFalse(os.path.exists(j(d, 'joined.txt')))

  def test_output_should_be_clobbered(self):
    with temp_dir() as d:
      with open(j(d, 'joined.txt'), 'wb') as joined:
        joined.write('garbage')
      with open(j(d, 'one.txt'), 'wb') as one:
        one.write('hello world\n')
      with open(j(d, 'two.txt'), 'wb') as two:
        two.write('line two ')
      with open(j(d, 'three.txt'), 'wb') as three:
        three.write('and three\n')
      exit_code = self.execute(d)
      self.assertEqual(0, exit_code)
      self.assertTrue(os.path.exists(j(d, 'joined.txt')))
      with open(j(d, 'joined.txt'), 'rb') as joined:
        joined_contents = joined.read()
      self.assertEqual(
        'hello world\nline two and three\n',
        joined_contents,
      )

if __name__ == '__main__':
  unittest.main()
