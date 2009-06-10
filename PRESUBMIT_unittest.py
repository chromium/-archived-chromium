#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for top-level Chromium presubmit script.
"""


import os
import PRESUBMIT
import re
import unittest


class MockInputApi(object):
  def __init__(self, test):
    self.affected_files = []
    self.re = re
    self.os_path = os.path
    self._test = test

  def AffectedFiles(self, include_deletes=True):
    if include_deletes:
      return self.affected_files
    else:
      return filter(lambda x: x.Action() != 'D', self.affected_files)

  def AffectedTextFiles(self, include_deletes=True):
    return self.affected_files

  def ReadFile(self, file):
    self._test.failIf(file.LocalPath().endswith('notsource'))
    return file.file_contents


class MockAffectedFile(object):
  def __init__(self, path, action='A'):
    self.path = path
    self.action = action

  def Action(self):
    return self.action

  def LocalPath(self):
    return self.path

  def AbsoluteLocalPath(self):
    return self.path


class MockOutputApi(object):
  class PresubmitError(object):
    def __init__(self, msg, items=[], long_text=''):
      self.msg = msg
      self.items = items


class PresubmitUnittest(unittest.TestCase):
  def testLocalChecks(self):
    api = MockInputApi(self)
    api.affected_files = [
      MockAffectedFile('foo/blat/yoo.notsource'),
      MockAffectedFile('third_party/blat/source.cc'),
      MockAffectedFile('foo/blat/source.h'),
      MockAffectedFile('foo/blat/source.mm'),
      MockAffectedFile('foo/blat/source.py'),
    ]
    for item in api.affected_files:
      item.file_contents = 'file with \n\terror\nhere\r\nyes there'
    # 3 source files, 2 errors by file + 1 global CR  + 1 global EOF error.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 8)

    for item in api.affected_files:
      item.file_contents = 'file\twith\ttabs\n'
    # 3 source files, 1 error by file.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 3)

    for item in api.affected_files:
      item.file_contents = 'file\rusing\rCRs\n'
    # One global CR error.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 1)
    self.failUnless(
      len(PRESUBMIT.LocalChecks(api, MockOutputApi)[0].items) == 3)

    for item in api.affected_files:
      item.file_contents = 'both\ttabs and\r\nCRLF\n'
    # 3 source files, 1 error by file + 1 global CR error.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 4)

    for item in api.affected_files:
      item.file_contents = 'file with\nzero \\t errors \\r\\n\n'
    self.failIf(PRESUBMIT.LocalChecks(api, MockOutputApi))

  def testLocalChecksDeletedFile(self):
    api = MockInputApi(self)
    api.affected_files = [
      MockAffectedFile('foo/blat/source.py', 'D'),
    ]
    api.affected_files[0].file_contents = (
        'file with \n\terror\nhere\r\nyes there')
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 0)


if __name__ == '__main__':
  unittest.main()
