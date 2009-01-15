# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares the text output of a test to the expected text output.

If the output doesn't match, returns FailureTextMismatch and outputs the diff
files into the layout test results directory.
"""

import errno
import logging
import os.path

from layout_package import path_utils
from layout_package import platform_utils
from layout_package import test_failures
from test_types import test_type_base

class TestTextDiff(test_type_base.TestTypeBase):
  def GetNormalizedOutputText(self, output):
    # Some tests produce "\r\n" explicitly.  Our system (Python/Cygwin)
    # helpfully changes the "\n" to "\r\n", resulting in "\r\r\n".
    norm = output.replace("\r\r\n", "\r\n").strip("\r\n").replace("\r\n", "\n")
    return norm + "\n"

  def GetNormalizedExpectedText(self, filename, show_sources):
    """Given the filename of the test, read the expected output from a file
    and normalize the text.  Returns a string with the expected text, or ''
    if the expected output file was not found."""
    # Read the platform-specific expected text.
    expected_filename = path_utils.ExpectedFilename(filename, '.txt',
                                                    self._platform)

    if show_sources:
      logging.debug('Using %s' % expected_filename)
    try:
      expected = open(expected_filename).read()
    except IOError, e:
      if errno.ENOENT != e.errno:
        raise
      expected = ''
      return expected

    # Normalize line endings
    return expected.strip("\r\n").replace("\r\n", "\n") + "\n"

  def CompareOutput(self, filename, proc, output, test_args, target):
    """Implementation of CompareOutput that checks the output text against the
    expected text from the LayoutTest directory."""
    failures = []

    # If we're generating a new baseline, we pass.
    if test_args.new_baseline:
      self._SaveBaselineData(filename, output, ".txt")
      return failures

    # Normalize text to diff
    output = self.GetNormalizedOutputText(output)
    expected = self.GetNormalizedExpectedText(filename, test_args.show_sources)

    # Write output files for new tests, too.
    if output != expected:
      # Text doesn't match, write output files.
      self.WriteOutputFiles(filename, "", ".txt", output, expected,
                            diff=True, wdiff=True)

      if expected == '':
        failures.append(test_failures.FailureMissingResult(self))
      else:
        failures.append(test_failures.FailureTextMismatch(self, True))

    return failures

