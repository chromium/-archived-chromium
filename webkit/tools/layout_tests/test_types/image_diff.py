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

"""Compares the image output of a test to the expected image output.

Compares hashes for the generated and expected images. If the output doesn't
match, returns FailureImageHashMismatch and outputs both hashes into the layout
test results directory.
"""

import errno
import os
import shutil

from layout_package import path_utils
from layout_package import test_failures
from test_types import test_type_base

class ImageDiff(test_type_base.TestTypeBase):
  def _CopyOutputPNGs(self, filename, actual_png, expected_png):
    """Copies result files into the output directory with appropriate names.

    Args:
      filename: the test filename
      actual_png: path to the actual result file
      expected_png: path to the expected result file
    """
    self._MakeOutputDirectory(filename)
    actual_filename = self.OutputFilename(filename, "-actual-win.png")
    expected_filename = self.OutputFilename(filename, "-expected.png")

    shutil.copyfile(actual_png, actual_filename)
    try:
      shutil.copyfile(expected_png, expected_filename)
    except IOError, e:
      # A missing expected PNG has already been recorded as an error.
      if errno.ENOENT != e.errno:
        raise

  def _SaveBaselineFiles(self, filename, dest_dir, png_path, checksum):
    """Saves new baselines for the PNG and checksum.

    Args:
      filename: test filename
      dest_dir: outer directory into which the results should be saved.
      png_path: path to the actual PNG result file
      checksum: value of the actual checksum result
    """
    png_file = open(png_path, "rb")
    png_data = png_file.read()
    png_file.close()
    self._SaveBaselineData(filename, dest_dir, png_data, ".png")
    self._SaveBaselineData(filename, dest_dir, checksum, ".checksum")

  def CompareOutput(self, filename, proc, output, test_args):
    """Implementation of CompareOutput that checks the output image and
    checksum against the expected files from the LayoutTest directory.
    """
    failures = []

    # If we didn't produce a hash file, this test must be text-only.
    if test_args.hash is None:
      return failures

    # If we're generating a new baseline, we pass.
    if test_args.new_baseline:
      self._SaveBaselineFiles(filename, test_args.new_baseline,
                              test_args.png_path, test_args.hash)
      return failures

    # Compare hashes.
    expected_hash_file = path_utils.ExpectedFilename(filename,
                                                     '.checksum',
                                                     self._custom_result_id)

    expected_png_file = path_utils.ExpectedFilename(filename,
                                                    '.png',
                                                    self._custom_result_id)

    try:
      expected_hash = open(expected_hash_file, "r").read()
    except IOError, e:
      if errno.ENOENT != e.errno:
        raise
      expected_hash = ''

    if test_args.hash != expected_hash:
      # TODO(pamg): If the hashes don't match, use the image_diff app to
      # compare the actual images, and report a different error if those do
      # match.
      if expected_hash == '':
        failures.append(test_failures.FailureMissingImageHash(self))
      else:
        failures.append(test_failures.FailureImageHashMismatch(self))

    # Also report a missing expected PNG file.
    if not os.path.isfile(expected_png_file):
      failures.append(test_failures.FailureMissingImage(self))

    # If anything was wrong, write the output files.
    if len(failures):
      self.WriteOutputFiles(filename, '', '.checksum', test_args.hash,
                            expected_hash, diff=False)
      self._CopyOutputPNGs(filename, test_args.png_path,
                          expected_png_file)

    return failures
