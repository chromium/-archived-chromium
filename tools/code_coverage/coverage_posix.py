#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate and process code coverage.

TODO(jrg): rename this from coverage_posix.py to coverage_all.py!

Written for and tested on Mac, Linux, and Windows.  To use this script
to generate coverage numbers, please run from within a gyp-generated
project.

All platforms, to set up coverage:
  cd ...../chromium ; src/tools/gyp/gyp_dogfood -Dcoverage=1 src/build/all.gyp

Run coverage on...
Mac:
  ( cd src/chrome ; xcodebuild -configuration Debug -target coverage )
Linux:
  ( cd src/chrome ; hammer coverage )
  # In particular, don't try and run 'coverage' from src/build


--directory=DIR: specify directory that contains gcda files, and where
  a "coverage" directory will be created containing the output html.
  Example name:   ..../chromium/src/xcodebuild/Debug

--genhtml: generate html output.  If not specified only lcov is generated.

--all_unittests: if present, run all files named *_unittests that we
  can find.

--fast_test: make the tests run real fast (just for testing)

--strict: if a test fails, we continue happily.  --strict will cause
  us to die immediately.

Strings after all options are considered tests to run.  Test names
have all text before a ':' stripped to help with gyp compatibility.
For example, ../base/base.gyp:base_unittests is interpreted as a test
named "base_unittests".
"""

import glob
import logging
import optparse
import os
import shutil
import subprocess
import sys

class Coverage(object):
  """Doitall class for code coverage."""

  def __init__(self, directory, options, args):
    super(Coverage, self).__init__()
    logging.basicConfig(level=logging.DEBUG)
    self.directory = directory
    self.options = options
    self.args = args
    self.directory_parent = os.path.dirname(self.directory)
    self.output_directory = os.path.join(self.directory, 'coverage')
    if not os.path.exists(self.output_directory):
      os.mkdir(self.output_directory)
    # The "final" lcov-format file
    self.coverage_info_file = os.path.join(self.directory, 'coverage.info')
    # If needed, an intermediate VSTS-format file
    self.vsts_output = os.path.join(self.directory, 'coverage.vsts')
    # Needed for Windows.
    self.src_root = options.src_root
    self.FindPrograms()
    self.ConfirmPlatformAndPaths()
    self.tests = []

  def FindInPath(self, program):
    """Find program in our path.  Return abs path to it, or None."""
    if not 'PATH' in os.environ:
      logging.fatal('No PATH environment variable?')
      sys.exit(1)
    paths = os.environ['PATH'].split(os.pathsep)
    for path in paths:
      fullpath = os.path.join(path, program)
      if os.path.exists(fullpath):
        return fullpath
    return None

  def FindPrograms(self):
    """Find programs we may want to run."""
    if self.IsPosix():
      self.lcov_directory = os.path.join(sys.path[0],
                                         '../../third_party/lcov/bin')
      self.lcov = os.path.join(self.lcov_directory, 'lcov')
      self.mcov = os.path.join(self.lcov_directory, 'mcov')
      self.genhtml = os.path.join(self.lcov_directory, 'genhtml')
      self.programs = [self.lcov, self.mcov, self.genhtml]
    else:
      commands = ['vsperfcmd.exe', 'vsinstr.exe', 'coverage_analyzer.exe']
      self.perf = self.FindInPath('vsperfcmd.exe')
      self.instrument = self.FindInPath('vsinstr.exe')
      self.analyzer = self.FindInPath('coverage_analyzer.exe')
      if not self.perf or not self.instrument or not self.analyzer:
        logging.fatal('Could not find Win performance commands.')
        logging.fatal('Commands needed in PATH: ' + str(commands))
        sys.exit(1)
      self.programs = [self.perf, self.instrument, self.analyzer]

  def FindTests(self):
    """Find unit tests to run; set self.tests to this list.

    Assume all non-option items in the arg list are tests to be run.
    """
    # Small tests: can be run in the "chromium" directory.
    # If asked, run all we can find.
    if self.options.all_unittests:
      self.tests += glob.glob(os.path.join(self.directory, '*_unittests'))

    # If told explicit tests, run those (after stripping the name as
    # appropriate)
    for testname in self.args:
      if ':' in testname:
        self.tests += [os.path.join(self.directory, testname.split(':')[1])]
      else:
        self.tests += [os.path.join(self.directory, testname)]
    # Medium tests?
    # Not sure all of these work yet (e.g. page_cycler_tests)
    # self.tests += glob.glob(os.path.join(self.directory, '*_tests'))

    # If needed, append .exe to tests since vsinstr.exe likes it that
    # way.
    if self.IsWindows():
      for ind in range(len(self.tests)):
        test = self.tests[ind]
        test_exe = test + '.exe'
        if not test.endswith('.exe') and os.path.exists(test_exe):
          self.tests[ind] = test_exe

    # Temporarily make Windows quick for bringup by filtering
    # out all except base_unittests.  Easier than a chrome.cyp change.
    # TODO(jrg): remove this
    if self.IsWindows():
      t2 = []
      for test in self.tests:
        if 'base_unittests' in test:
          t2.append(test)
      self.tests = t2



  def ConfirmPlatformAndPaths(self):
    """Confirm OS and paths (e.g. lcov)."""
    for program in self.programs:
      if not os.path.exists(program):
        logging.fatal('Program missing: ' + program)
        sys.exit(1)

  def Run(self, cmdlist, ignore_error=False, ignore_retcode=None,
          explanation=None):
    """Run the command list; exit fatally on error."""
    logging.info('Running ' + str(cmdlist))
    retcode = subprocess.call(cmdlist)
    if retcode:
      if ignore_error or retcode == ignore_retcode:
        logging.warning('COVERAGE: %s unhappy but errors ignored  %s' %
                        (str(cmdlist), explanation or ''))
      else:
        logging.fatal('COVERAGE:  %s failed; return code: %d' %
                      (str(cmdlist), retcode))
        sys.exit(retcode)


  def IsPosix(self):
    """Return True if we are POSIX."""
    return self.IsMac() or self.IsLinux()

  def IsMac(self):
    return sys.platform == 'darwin'

  def IsLinux(self):
    return sys.platform == 'linux2'

  def IsWindows(self):
    """Return True if we are Windows."""
    return sys.platform in ('win32', 'cygwin')

  def ClearData(self):
    """Clear old gcda files"""
    if not self.IsPosix():
      return
    subprocess.call([self.lcov,
                     '--directory', self.directory_parent,
                     '--zerocounters'])
    shutil.rmtree(os.path.join(self.directory, 'coverage'))

  def BeforeRunTests(self):
    """Do things before running tests."""
    if not self.IsWindows():
      return
    # Stop old counters if needed
    cmdlist = [self.perf, '-shutdown']
    self.Run(cmdlist, ignore_error=True)
    # Instrument binaries
    for fulltest in self.tests:
      if os.path.exists(fulltest):
        cmdlist = [self.instrument, '/COVERAGE', fulltest]
        self.Run(cmdlist, ignore_retcode=4,
                 explanation='OK with a multiple-instrument')
    # Start new counters
    cmdlist = [self.perf, '-start:coverage', '-output:' + self.vsts_output]
    self.Run(cmdlist)

  def RunTests(self):
    """Run all unit tests."""
    for fulltest in self.tests:
      if not os.path.exists(fulltest):
        logging.fatal(fulltest + ' does not exist')
        if self.options.strict:
          sys.exit(2)
      # TODO(jrg): add timeout?
      print >>sys.stderr, 'Running test: ' + fulltest
      cmdlist = [fulltest, '--gtest_print_time']

      # If asked, make this REAL fast for testing.
      if self.options.fast_test:
        # cmdlist.append('--gtest_filter=RenderWidgetHost*')
        cmdlist.append('--gtest_filter=CommandLine*')

      retcode = subprocess.call(cmdlist)
      if retcode:
        logging.fatal('COVERAGE: test %s failed; return code: %d' %
                      (fulltest, retcode))
        if self.options.strict:
          sys.exit(retcode)

  def AfterRunTests(self):
    """Do things right after running tests."""
    if not self.IsWindows():
      return
    # Stop counters
    cmdlist = [self.perf, '-shutdown']
    self.Run(cmdlist)
    full_output = self.vsts_output + '.coverage'
    shutil.move(full_output, self.vsts_output)

  def GenerateLcovPosix(self):
    """Convert profile data to lcov."""
    command = [self.mcov,
               '--directory', self.directory_parent,
               '--output', self.coverage_info_file]
    print >>sys.stderr, 'Assembly command: ' + ' '.join(command)
    retcode = subprocess.call(command)
    if retcode:
      logging.fatal('COVERAGE: %s failed; return code: %d' %
                    (command[0], retcode))
      if self.options.strict:
        sys.exit(retcode)

  def GenerateLcovWindows(self):
    """Convert VSTS format to lcov."""
    lcov_file = self.vsts_output + '.lcov'
    if os.path.exists(lcov_file):
      os.remove(lcov_file)
    # generates the file (self.vsts_output + ".lcov")

    cmdlist = [self.analyzer,
               '-sym_path=' + self.directory,
               '-src_root=' + self.src_root,
               self.vsts_output]
    self.Run(cmdlist)
    if not os.path.exists(lcov_file):
      logging.fatal('Output file %s not created' % lcov_file)
      sys.exit(1)
    # So we name it appropriately
    if os.path.exists(self.coverage_info_file):
      os.remove(self.coverage_info_file)
    logging.info('Renaming LCOV file to %s to be consistent' %
                 self.coverage_info_file)
    shutil.move(self.vsts_output + '.lcov', self.coverage_info_file)

  def GenerateLcov(self):
    if self.IsPosix():
      self.GenerateLcovPosix()
    else:
      self.GenerateLcovWindows()

  def GenerateHtml(self):
    """Convert lcov to html."""
    # TODO(jrg): This isn't happy when run with unit_tests since V8 has a
    # different "base" so V8 includes can't be found in ".".  Fix.
    command = [self.genhtml,
               self.coverage_info_file,
               '--output-directory',
               self.output_directory]
    print >>sys.stderr, 'html generation command: ' + ' '.join(command)
    retcode = subprocess.call(command)
    if retcode:
      logging.fatal('COVERAGE: %s failed; return code: %d' %
                    (command[0], retcode))
      if self.options.strict:
        sys.exit(retcode)

def main():
  # Print out the args to help someone do it by hand if needed
  print >>sys.stderr, sys.argv

  parser = optparse.OptionParser()
  parser.add_option('-d',
                    '--directory',
                    dest='directory',
                    default=None,
                    help='Directory of unit test files')
  parser.add_option('-a',
                    '--all_unittests',
                    dest='all_unittests',
                    default=False,
                    help='Run all tests we can find (*_unittests)')
  parser.add_option('-g',
                    '--genhtml',
                    dest='genhtml',
                    default=False,
                    help='Generate html from lcov output')
  parser.add_option('-f',
                    '--fast_test',
                    dest='fast_test',
                    default=False,
                    help='Make the tests run REAL fast by doing little.')
  parser.add_option('-s',
                    '--strict',
                    dest='strict',
                    default=False,
                    help='Be strict and die on test failure.')
  parser.add_option('-S',
                    '--src_root',
                    dest='src_root',
                    default='.',
                    help='Source root (only used on Windows)')
  (options, args) = parser.parse_args()
  if not options.directory:
    parser.error('Directory not specified')
  coverage = Coverage(options.directory, options, args)
  coverage.ClearData()
  coverage.FindTests()
  coverage.BeforeRunTests()
  coverage.RunTests()
  coverage.AfterRunTests()
  coverage.GenerateLcov()
  if options.genhtml:
    coverage.GenerateHtml()
  return 0


if __name__ == '__main__':
  sys.exit(main())
