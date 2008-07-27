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

"""Compares the text output of a test to the expected text output.

If the output doesn't match, returns FailureTextMismatch and outputs the diff
files into the layout test results directory.
"""

import errno
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

  def GetNormalizedExpectedText(self, filename):
    """Given the filename of the test, read the expected output from a file
    and normalize the text.  Returns a string with the expected text, or ''
    if the expected output file was not found."""
    # Read the platform-specific expected text, or the Mac default if no
    # platform result exists.
    expected_filename = path_utils.ExpectedFilename(filename, '.txt',
                                                    self._custom_result_id)
    try:
      expected = open(expected_filename).read()
    except IOError, e:
      if errno.ENOENT != e.errno:
        raise
      expected = ''
      return expected

    # Normalize line endings
    return expected.strip("\r\n") + "\n"

  def CompareOutput(self, filename, proc, output, test_args):
    """Implementation of CompareOutput that checks the output text against the
    expected text from the LayoutTest directory."""
    failures = []

    # If we're generating a new baseline, we pass.
    if test_args.text_baseline:
      self._SaveBaselineData(filename, test_args.new_baseline,
                             output, ".txt")
      return failures

    # Normalize text to diff
    output = self.GetNormalizedOutputText(output)
    expected = self.GetNormalizedExpectedText(filename)

    # Write output files for new tests, too.
    if output != expected:
      # Text doesn't match, write output files.
      self.WriteOutputFiles(filename, "", ".txt", output, expected)

      if expected == '':
        failures.append(test_failures.FailureMissingResult(self))
      else:
        failures.append(test_failures.FailureTextMismatch(self))

    return failures
