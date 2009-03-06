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
  def __init__(self):
    self.affected_files = []
    self.re = re
    self.os_path = os.path

  def AffectedFiles(self):
    return self.affected_files

  def AffectedTextFiles(self, include_deletes=True):
    return self.affected_files


class MockAffectedFile(object):
  def __init__(self, path):
    self.path = path

  def LocalPath(self):
    return self.path


class MockOutputApi(object):
  class PresubmitError(object):
    def __init__(self, msg, items=[], long_text=''):
      self.msg = msg
      self.items = items


class PresubmitUnittest(unittest.TestCase):
  def setUp(self):
    self.file_contents = ''
    def MockReadFile(path):
      self.failIf(path.endswith('notsource'))
      return self.file_contents
    PRESUBMIT._ReadFile = MockReadFile

  def tearDown(self):
    PRESUBMIT._ReadFile = PRESUBMIT.ReadFile

  def testLocalChecks(self):
    api = MockInputApi()
    api.affected_files = [
      MockAffectedFile('bleh/blat/yoo.notsource'),
      MockAffectedFile('third_party/blat/source.cc'),
      MockAffectedFile('bleh/blat/source.h'),
      MockAffectedFile('bleh/blat/source.mm'),
      MockAffectedFile('bleh/blat/source.py'),
    ]
    self.file_contents = 'file with \n\terror\nhere\r\nyes there'
    # 3 source files, 2 errors by file + 1 global CR error.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 7)

    self.file_contents = 'file\twith\ttabs'
    # 3 source files, 1 error by file.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 3)

    self.file_contents = 'file\rusing\rCRs'
    # One global CR error.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 1)
    self.failUnless(
      len(PRESUBMIT.LocalChecks(api, MockOutputApi)[0].items) == 3)

    self.file_contents = 'both\ttabs and\r\nCRLF'
    # 3 source files, 1 error by file + 1 global CR error.
    self.failUnless(len(PRESUBMIT.LocalChecks(api, MockOutputApi)) == 4)

    self.file_contents = 'file with\nzero \\t errors \\r\\n'
    self.failIf(PRESUBMIT.LocalChecks(api, MockOutputApi))


if __name__ == '__main__':
  unittest.main()
