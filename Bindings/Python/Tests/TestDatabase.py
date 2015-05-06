#!/usr/bin/env python2.7

import _b
import contextlib
import os.path
import shutil
import tempfile
import unittest

@contextlib.contextmanager
def temp_dir():
  dir = tempfile.mkdtemp()
  try:
    yield dir
  finally:
    shutil.rmtree(dir)

class TestDatabase(unittest.TestCase):
  def test_open_sqlite3_file(self):
    with temp_dir() as d:
      path = os.path.join(d, 'TestDatabase.cache')
      with _b.Database.open_sqlite3(
        sqlite_path=path,
        sqlite_flags=_b.Database.SQLITE_OPEN_READWRITE
          | _b.Database.SQLITE_OPEN_CREATE,
        sqlite_vfs=None,
      ) as db:
        self.assertTrue(os.path.exists(path))
      self.assertTrue(os.path.exists(path))
      # Open without SQLITE_OPEN_CREATE.
      with _b.Database.open_sqlite3(
        sqlite_path=path,
        sqlite_flags=_b.Database.SQLITE_OPEN_READWRITE,
        sqlite_vfs=None,
      ) as db:
        self.assertTrue(os.path.exists(path))
      self.assertTrue(os.path.exists(path))

  def test_open_sqlite3_memory(self):
    with temp_dir() as d:
      with _b.Database.open_sqlite3(
        sqlite_path=':memory:',
        sqlite_flags=_b.Database.SQLITE_OPEN_READWRITE
          | _b.Database.SQLITE_OPEN_CREATE,
        sqlite_vfs=None,
      ) as db:
        self.assertFalse(os.path.exists(':memory:'))
      self.assertFalse(os.path.exists(':memory:'))

if __name__ == '__main__':
  unittest.main()
