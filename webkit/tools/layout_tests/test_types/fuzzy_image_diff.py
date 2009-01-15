# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compares the image output of a test to the expected image output using
fuzzy matching.
"""

import errno
import logging
import os
import shutil
import subprocess

from layout_package import path_utils
from layout_package import test_failures
from test_types import test_type_base

class FuzzyImageDiff(test_type_base.TestTypeBase):
  def CompareOutput(self, filename, proc, output, test_args, target):
    """Implementation of CompareOutput that checks the output image and
    checksum against the expected files from the LayoutTest directory.
    """
    failures = []

    # If we didn't produce a hash file, this test must be text-only.
    if test_args.hash is None:
      return failures

    expected_png_file = path_utils.ExpectedFilename(filename,
                                                    '.png',
                                                    self._platform)

    if test_args.show_sources:
      logging.debug('Using %s' % expected_png_file)

    # Also report a missing expected PNG file.
    if not os.path.isfile(expected_png_file):
      failures.append(test_failures.FailureMissingImage(self))

    # Run the fuzzymatcher
    r = subprocess.call([path_utils.GetPlatformUtil().FuzzyMatchBinaryPath(),
                        test_args.png_path, expected_png_file])
    if r != 0:
      failures.append(test_failures.FailureFuzzyFailure(self))

    return failures

