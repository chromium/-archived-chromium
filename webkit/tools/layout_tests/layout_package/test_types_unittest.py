#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests that verify that the various test_types (e.g., simplified_diff)
are working."""

import difflib
import os
import unittest

from test_types import simplified_text_diff

class SimplifiedDiffUnittest(unittest.TestCase):
  def testSimplifiedDiff(self):
    """Compares actual output with expected output for some test cases.  The
    simplified diff of these test cases should be the same."""
    test_names = [
      'null-offset-parent',
      'textAreaLineHeight',
      'form-element-geometry',
    ]
    differ = simplified_text_diff.SimplifiedTextDiff(None)
    for prefix in test_names:
      output_filename = os.path.join(self.GetTestDataDir(),
                                     prefix + "-actual-win.txt")
      expected_filename = os.path.join(self.GetTestDataDir(),
                                       prefix + "-expected.txt")
      output = differ._SimplifyText(open(output_filename).read())
      expected = differ._SimplifyText(open(expected_filename).read())

      if output != expected:
        lst = difflib.unified_diff(expected.splitlines(True),
                                   output.splitlines(True),
                                   'expected',
                                   'actual')
        for line in lst:
          print line.rstrip()
      self.failUnlessEqual(output, expected)

  def GetTestDataDir(self):
    return os.path.join(os.path.abspath('testdata'), 'difftests')

if '__main__' == __name__:
  unittest.main()

