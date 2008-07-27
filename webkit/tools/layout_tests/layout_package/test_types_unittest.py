#!/bin/env python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
