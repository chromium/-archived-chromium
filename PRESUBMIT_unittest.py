#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for top-level Chromium presubmit script.
"""


import PRESUBMIT
import unittest


class MockInputApi(object):
  def __init__(self):
    self.affected_files = []
  
  def AffectedTextFiles(self, include_deletes=True):
    return self.affected_files


class MockAffectedFile(object):
  def __init__(self, path):
    self.path = path
  
  def LocalPath(self):
    return self.path


class MockOutputApi(object):
  class PresubmitError(object):
    def __init__(self, msg, items):
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
  
  def testCheckNoCrLfOrTabs(self):
    api = MockInputApi()
    api.affected_files = [
      MockAffectedFile('foo/blat/yoo.notsource'),
      MockAffectedFile('foo/blat/source.h'),
      MockAffectedFile('foo/blat/source.mm'),
      MockAffectedFile('foo/blat/source.py'),
    ]
    self.file_contents = 'file with\nerror\nhere\r\nyes there'
    self.failUnless(len(PRESUBMIT.CheckNoCrOrTabs(api, MockOutputApi)) == 1)
    self.failUnless(
      len(PRESUBMIT.CheckNoCrOrTabs(api, MockOutputApi)[0].items) == 3)
    
    self.file_contents = 'file\twith\ttabs'
    self.failUnless(len(PRESUBMIT.CheckNoCrOrTabs(api, MockOutputApi)) == 1)
    
    self.file_contents = 'file\rusing\rCRs'
    self.failUnless(len(PRESUBMIT.CheckNoCrOrTabs(api, MockOutputApi)) == 1)
    
    self.file_contents = 'both\ttabs and\r\nCRLF'
    self.failUnless(len(PRESUBMIT.CheckNoCrOrTabs(api, MockOutputApi)) == 2)
    
    self.file_contents = 'file with\nzero \\t errors \\r\\n'
    self.failIf(PRESUBMIT.CheckNoCrOrTabs(api, MockOutputApi))


if __name__ == '__main__':
  unittest.main()
