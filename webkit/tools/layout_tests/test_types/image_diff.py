# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares the image output of a test to the expected image output.

Compares hashes for the generated and expected images. If the output doesn't
match, returns FailureImageHashMismatch and outputs both hashes into the layout
test results directory.
"""

import errno
import logging
import os
import shutil
import subprocess

from layout_package import path_utils
from layout_package import platform_utils
from layout_package import test_failures
from test_types import test_type_base

# Cache whether we have the image_diff executable available.
_compare_available = True
_compare_msg_printed = False

class ImageDiff(test_type_base.TestTypeBase):
  def _CopyOutputPNGs(self, filename, actual_png, expected_png):
    """Copies result files into the output directory with appropriate names.

    Args:
      filename: the test filename
      actual_png: path to the actual result file
      expected_png: path to the expected result file
    """
    self._MakeOutputDirectory(filename)
    actual_filename = self.OutputFilename(filename, "-actual.png")
    expected_filename = self.OutputFilename(filename, "-expected.png")

    shutil.copyfile(actual_png, actual_filename)
    try:
      shutil.copyfile(expected_png, expected_filename)
    except IOError, e:
      # A missing expected PNG has already been recorded as an error.
      if errno.ENOENT != e.errno:
        raise

  def _SaveBaselineFiles(self, filename, png_path, checksum):
    """Saves new baselines for the PNG and checksum.

    Args:
      filename: test filename
      png_path: path to the actual PNG result file
      checksum: value of the actual checksum result
    """
    png_file = open(png_path, "rb")
    png_data = png_file.read()
    png_file.close()
    self._SaveBaselineData(filename, png_data, ".png")
    self._SaveBaselineData(filename, checksum, ".checksum")

  def _CreateImageDiff(self, filename, target):
    """Creates the visual diff of the expected/actual PNGs.

    Args:
      filename: the name of the test
      target: Debug or Release
    """
    diff_filename = self.OutputFilename(filename,
      self.FILENAME_SUFFIX_COMPARE)
    actual_filename = self.OutputFilename(filename,
      self.FILENAME_SUFFIX_ACTUAL + '.png')
    expected_filename = self.OutputFilename(filename,
      self.FILENAME_SUFFIX_EXPECTED + '.png')
    platform_util = platform_utils.PlatformUtility('')

    global _compare_available
    cmd = ''

    try:
      executable = platform_util.ImageCompareExecutablePath(target)
      cmd = [executable, '--diff', actual_filename, expected_filename,
             diff_filename]
    except Exception, e:
      _compare_available = False

    result = 1
    if _compare_available:
      try:
        result = subprocess.call(cmd);
      except OSError, e:
        if e.errno == errno.ENOENT or e.errno == errno.EACCES:
          _compare_available = False
        else:
          raise e

    global _compare_msg_printed

    if not _compare_available and not _compare_msg_printed:
      _compare_msg_printed = True
      print('image_diff not found. Make sure you have a ' + target +
            ' build of the image_diff executable.')

    return result

  def CompareOutput(self, filename, proc, output, test_args, target):
    """Implementation of CompareOutput that checks the output image and
    checksum against the expected files from the LayoutTest directory.
    """
    failures = []

    # If we didn't produce a hash file, this test must be text-only.
    if test_args.hash is None:
      return failures

    # If we're generating a new baseline, we pass.
    if test_args.new_baseline:
      self._SaveBaselineFiles(filename, test_args.png_path, test_args.hash)
      return failures

    # Compare hashes.
    expected_hash_file = path_utils.ExpectedFilename(filename,
                                                     '.checksum',
                                                     self._platform)

    expected_png_file = path_utils.ExpectedFilename(filename,
                                                    '.png',
                                                    self._platform)

    if test_args.show_sources:
      logging.debug('Using %s' % expected_png_file)

    try:
      expected_hash = open(expected_hash_file, "r").read()
    except IOError, e:
      if errno.ENOENT != e.errno:
        raise
      expected_hash = ''

    if test_args.hash != expected_hash:
      if expected_hash == '':
        failures.append(test_failures.FailureMissingImageHash(self))
      else:
        # Hashes don't match, so see if the images match.  If they do, then
        # the hash is wrong.
        self._CopyOutputPNGs(filename, test_args.png_path,
                             expected_png_file)
        result = self._CreateImageDiff(filename, target)
        if result == 0:
          failures.append(test_failures.FailureImageHashIncorrect(self))
        else:
          failures.append(test_failures.FailureImageHashMismatch(self))

    # Also report a missing expected PNG file.
    if not os.path.isfile(expected_png_file):
      failures.append(test_failures.FailureMissingImage(self))

    # If anything was wrong, write the output files.
    if len(failures):
      self.WriteOutputFiles(filename, '', '.checksum', test_args.hash,
                            expected_hash, diff=False, wdiff=False)

      if (test_args.hash == expected_hash or expected_hash == ''):
        self._CopyOutputPNGs(filename, test_args.png_path,
                             expected_png_file)
        self._CreateImageDiff(filename, target)

    return failures
