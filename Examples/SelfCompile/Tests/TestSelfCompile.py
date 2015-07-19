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

class TestSelfCompile(unittest.TestCase):
  def setUp(self):
    exe = os.getenv('SELF_COMPILE_EXE')
    if exe is None:
      raise Exception(
        'Define the SELF_COMPILE_EXE environment variable '
        'to be the path of your executable',
      )
    self.exe = os.path.realpath(exe)
    source_dir = os.getenv('CODE_DIR')
    if source_dir is None:
      raise Exception(
        'Define the CODE_DIR environment variable to the '
        'path containing B\'s source code',
      )
    self.source_dir = source_dir
    self.run_test_sh = os.getenv('B_RUN_TEST_SH')

  def execute(self, temp_dir, extra_args=[]):
    args = [self.exe] + extra_args
    if self.run_test_sh is not None:
      args = [self.run_test_sh] + args
    if os.getenv('B_DEBUG_TEST'):
      print('Would execute: {}'.format(
        ' '.join(map(pipes.quote, args)),
      ))
      print('In directory : {}'.format(temp_dir))
      pdb.set_trace()
    return subprocess.call(args, cwd=temp_dir)

  def test_full_build(self):
    with temp_dir() as d:
      tree = os.path.join(d, 'tree')
      shutil.copytree(self.source_dir, tree)
      exit_code = self.execute(tree)
      self.assertEqual(0, exit_code)
      self.assertTrue(
        os.path.exists(os.path.join(tree, 'SelfCompile')),
      )

  def test_single_missing_source_error(self):
    with temp_dir() as d:
      tree = os.path.join(d, 'tree')
      shutil.copytree(self.source_dir, tree)
      os.remove(os.path.join(tree, 'Source/Assertions.c'))
      exit_code = self.execute(tree)
      self.assertEqual(2, exit_code)
      self.assertFalse(os.path.exists(
        os.path.join(tree, 'Source/Assertions.o'),
      ))
      self.assertFalse(os.path.exists(
        os.path.join(tree, 'SelfCompile'),
      ))

  def test_single_compile_error(self):
    with temp_dir() as d:
      tree = os.path.join(d, 'tree')
      shutil.copytree(self.source_dir, tree)
      with open(
        os.path.join(tree, 'Source/Assertions.c'),
        'w',
      ) as f:
        f.write('#error "Intentional compile error"\n');
      exit_code = self.execute(tree)
      self.assertEqual(2, exit_code)
      self.assertFalse(os.path.exists(
        os.path.join(tree, 'Source/Assertions.o'),
      ))
      self.assertFalse(os.path.exists(
        os.path.join(tree, 'SelfCompile'),
      ))

  def test_single_linker_error(self):
    with temp_dir() as d:
      tree = os.path.join(d, 'tree')
      shutil.copytree(self.source_dir, tree)
      with open(
        os.path.join(tree, 'Source/Assertions.c'),
        'w',
      ) as f:
        # Assertions.c exports a definition. Replace it with
        # a dummy definition.
        f.write('void b_test_self_compile_func(void);');
        f.write('void b_test_self_compile_func(void) {}');
      exit_code = self.execute(tree)
      self.assertEqual(2, exit_code)
      self.assertTrue(os.path.exists(
        os.path.join(tree, 'Source/Assertions.o'),
      ))
      self.assertFalse(os.path.exists(
        os.path.join(tree, 'SelfCompile'),
      ))

if __name__ == '__main__':
  unittest.main()
