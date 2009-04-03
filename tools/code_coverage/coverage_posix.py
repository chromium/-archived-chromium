#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate and process code coverage on POSIX systems.

Written for and tested on Mac.
Not tested on Linux yet.

--directory=DIR: specify directory that contains gcda files, and where
  a "coverage" directory will be created containing the output html.

TODO(jrg): make list of unit tests an arg to this script
"""

import logging
import optparse
import os
import subprocess
import sys

class Coverage(object):
  """Doitall class for code coverage."""

  # Unit test files to run.
  UNIT_TESTS = [
    'base_unittests',
    # 'unit_tests,
    ]

  def __init__(self, directory):
    super(Coverage, self).__init__()
    self.directory = directory
    self.output_directory = os.path.join(self.directory, 'coverage')
    if not os.path.exists(self.output_directory):
      os.mkdir(self.output_directory)
    self.lcov_directory = os.path.join(sys.path[0],
                                       '../../third_party/lcov/bin')
    self.lcov = os.path.join(self.lcov_directory, 'lcov')
    self.mcov = os.path.join(self.lcov_directory, 'mcov')
    self.genhtml = os.path.join(self.lcov_directory, 'genhtml')
    self.coverage_info_file = self.directory + 'coverage.info'
    self.ConfirmPlatformAndPaths()

  def ConfirmPlatformAndPaths(self):
    """Confirm OS and paths (e.g. lcov)."""
    if not self.IsPosix():
      logging.fatal('Not posix.')
    programs = [self.lcov, self.genhtml]
    if self.IsMac():
      programs.append(self.mcov)
    for program in programs:
      if not os.path.exists(program):
        logging.fatal('lcov program missing: ' + program)

  def IsPosix(self):
    """Return True if we are POSIX."""
    return self.IsMac() or self.IsLinux()

  def IsMac(self):
    return sys.platform == 'darwin'

  def IsLinux(self):
    return sys.platform == 'linux2'

  def ClearData(self):
    """Clear old gcda files"""
    subprocess.call([self.lcov,
                     '--directory', self.directory,
                     '--zerocounters'])

  def RunTests(self):
    """Run all unit tests."""
    for test in self.UNIT_TESTS:
      fulltest = os.path.join(self.directory, test)
      if not os.path.exists(fulltest):
        logging.fatal(fulltest + ' does not exist')
      # TODO(jrg): add timeout?
      # TODO(jrg): check return value and choke if it failed?
      # TODO(jrg): add --gtest_print_time like as run from XCode?
      subprocess.call([fulltest])

  def GenerateOutput(self):
    """Convert profile data to html."""
    if self.IsLinux():
      subprocess.call([self.lcov,
                       '--directory', self.directory,
                       '--capture',
                       '--output-file',
                       self.coverage_info_file])
    else:
      subprocess.call([self.mcov,
                       '--directory', self.directory,
                       '--output', self.coverage_info_file])
    subprocess.call([self.genhtml,
                     self.coverage_info_file,
                     '--output-directory',
                     self.output_directory])

def main():
  parser = optparse.OptionParser()
  parser.add_option('-d',
                    '--directory',
                    dest='directory',
                    default=None,
                    help='Directory of unit test files')
  (options, args) = parser.parse_args()
  if not options.directory:
    parser.error('Directory not specified')

  coverage = Coverage(options.directory)
  coverage.ClearData()
  coverage.RunTests()
  coverage.GenerateOutput()
  return 0


if __name__ == '__main__':
  sys.exit(main())
