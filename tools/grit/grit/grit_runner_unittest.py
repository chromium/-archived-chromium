#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.py'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '..'))

import unittest
import StringIO

from grit import util
import grit.grit_runner

class OptionArgsUnittest(unittest.TestCase):
  def setUp(self):
    self.buf = StringIO.StringIO()
    self.old_stdout = sys.stdout
    sys.stdout = self.buf

  def tearDown(self):
    sys.stdout = self.old_stdout

  def testSimple(self):
    grit.grit_runner.Main(['-i',
                           util.PathFromRoot('grit/test/data/simple-input.xml'),
                           '-d', 'test', 'bla', 'voff', 'ga'])
    output = self.buf.getvalue()
    self.failUnless(output.count('disconnected'))
    self.failUnless(output.count("'test'") == 0)  # tool name doesn't occur
    self.failUnless(output.count('bla'))
    self.failUnless(output.count('simple-input.xml'))


if __name__ == '__main__':
  unittest.main()
