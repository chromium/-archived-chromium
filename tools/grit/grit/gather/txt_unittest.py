#!/usr/bin/python2.4
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for TxtFile gatherer'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../..'))


import StringIO
import unittest

from grit.gather import txt


class TxtUnittest(unittest.TestCase):
  def testGather(self):
    input = StringIO.StringIO('Hello there\nHow are you?')
    gatherer = txt.TxtFile.FromFile(input)
    gatherer.Parse()
    self.failUnless(gatherer.GetText() == input.getvalue())
    self.failUnless(len(gatherer.GetCliques()) == 1)
    self.failUnless(gatherer.GetCliques()[0].GetMessage().GetRealContent() ==
                    input.getvalue())


if __name__ == '__main__':
  unittest.main()

