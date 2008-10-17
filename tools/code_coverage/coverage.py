#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Module to setup and generate code coverage data

This module first sets up the environment for code coverage, instruments the 
binaries, runs the tests and collects the code coverage data.


Usage:
  coverage.py --upload=<upload_location> 
              --revision=<revision_number>
              --src_root=<root_of_source_tree>
              [--tools_path=<tools_path>]
"""

import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

import google.logging_utils
import google.process_utils as proc


# The list of binaries that will be instrumented for code coverage
windows_binaries = ['unit_tests.exe',
                    'ui_tests.exe',
                    'ipc_tests.exe',
                    'memory_test.exe',
                    'net_perftests.exe',
                    'net_unittests.exe',
                    'page_cycler_tests.exe',
                    'perf_tests.exe',
                    'plugin_tests.exe',
                    'selenium_tests.exe',
                    'tab_switching_test.exe',
                    'test_shell_tests.exe',
                    'test_shell.exe',
                    'activex_test_control.dll']
                  
# The list of tests that will be run
windows_tests = ['unit_tests.exe',
                 'ui_tests.exe',
                 'ipc_tests.exe',
                 'net_perftests.exe',
                 'net_unittests.exe',
                 'plugin_tests.exe',
                 'tab_switching_test.exe',
                 'test_shell_tests.exe']


def IsWindows():
  """Checks if the current platform is Windows.
  """
  return sys.platform[:3] == 'win'


class Coverage(object):
  """Class to set up and generate code coverage.

  This class contains methods that are useful to set up the environment for 
  code coverage.

  Attributes:
    instrumented: A boolean indicating if all the binaries have been
                  instrumented.
  """

  def __init__(self, 
               revision,
               src_path = None,
               tools_path = None):
    """Init method for the Coverage class.

    Args:
      revision: Revision number of the Chromium source tree.
      src_path: Location of the Chromium source base.
      tools_path: Location of the Visual Studio Team Tools. (Win32 only)
    """
    google.logging_utils.config_root()
    self.revision = revision
    self.instrumented = False
    self.tools_path = tools_path
    self.src_path = src_path
    self._dir = None
  
  
  def SetUp(self, binaries):
    """Set up the platform specific environment and instrument the binaries for
    coverage.

    This method sets up the environment, instruments all the compiled binaries 
    and sets up the code coverage counters.

    Args:
      binaries: List of binaries that need to be instrumented.

    Returns:
      Path of the file containing code coverage data on successful
      instrumentation.
      None on error.
    """
    if self.instrumented:
      logging.error('Binaries already instrumented')
      return None
    coverage_file = None
    if IsWindows():
      # Stop all previous instance of VSPerfMon counters
      counters_command = ('%s -shutdown' % 
                          (os.path.join(self.tools_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      # TODO(niranjan): Add a check that to verify that the binaries were built
      # using the /PROFILE linker flag.
      if self.tools_path == None:
        logging.error('Could not locate Visual Studio Team Server tools')
        return None
      # Remove trailing slashes
      self.tools_path = self.tools_path.rstrip('\\') 
      instrument_command = '%s /COVERAGE ' % (os.path.join(self.tools_path,
                                                           'vsinstr.exe'))
      for binary in binaries:
        logging.info('binary = %s' % (binary))
        logging.info('instrument_command = %s' % (instrument_command))
        # Instrument each binary in the list
        binary = os.path.join(self.src_path, 'chrome', 'Release', binary)
        (retcode, output) = proc.RunCommandFull(instrument_command + binary,
                                                collect_output=True)
        # Check if the file has been instrumented correctly.
        if output.pop().rfind('Successfully instrumented') == -1:
          logging.error('Error instrumenting %s' % (binary))
          return None

      # Generate the file name for the coverage results
      self._dir = tempfile.mkdtemp()
      coverage_file = os.path.join(self._dir, 'chrome_win32_%s.coverage' % 
                                              (self.revision))
      logging.info('.coverage file: %s' % (coverage_file))

      # After all the binaries have been instrumented, we start the counters.
      counters_command = ('%s -start:coverage -output:%s' %
                          (os.path.join(self.tools_path, 'vsperfcmd.exe'),
                          coverage_file))
      # Here we use subprocess.call() instead of the RunCommandFull because the
      # VSPerfCmd spawns another process before terminating and this confuses
      # the subprocess.Popen() used by RunCommandFull.
      retcode = subprocess.call(counters_command)
      # TODO(niranjan): Check whether the counters have been started
      # successfully.
      
      # We are now ready to run tests and measure code coverage.
      self.instrumented = True
    else:
      return None
    return coverage_file
      

  def TearDown(self):
    """Tear down method.

    This method shuts down the counters, and cleans up all the intermediate
    artifacts. 
    """
    if self.instrumented == False:
      return
    
    if IsWindows():
      # Stop counters
      counters_command = ('%s -shutdown' % 
                         (os.path.join(self.tools_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      logging.info('Counters shut down: %s' % (output))
      # TODO(niranjan): Revert the instrumented binaries to their original
      # versions.
    else:
      return
    # Delete all the temp files and folders
    if self._dir != None:
      shutil.rmtree(self._dir, ignore_errors=True)
      logging.info('Cleaned up temporary files and folders')
    # Reset the instrumented flag.
    self.instrumented = False
    

  def Upload(self, coverage_file, upload_path, sym_path=None, src_root=None):
    """Upload the results to the dashboard.

    This method uploads the coverage data to a dashboard where it will be
    processed. On Windows, this method will first convert the .coverage file to
    the lcov format. This method needs to be called before the TearDown method.

    Args:
      coverage_file: The coverage data file to upload.
      upload_path: Destination where the coverage data will be processed.
      sym_path: Symbol path for the build (Win32 only)
      src_root: Root folder of the source tree (Win32 only)
    
    Returns:
      True on success.
      False on failure.
    """
    if IsWindows():
      # Stop counters
      counters_command = ('%s -shutdown' % 
                          (os.path.join(self.tools_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      logging.info('Counters shut down: %s' % (output))
      # Convert the .coverage file to lcov format
      if self.tools_path == None:
        logging.error('Lcov converter tool not found')
        return False
      self.tools_path = self.tools_path.rstrip('\\')
      convert_command = ('%s -sym_path=%s -src_root=%s %s' % 
                         (os.path.join(self.tools_path, 
                                       'coverage_analyzer.exe'),
                         sym_path,
                         src_root,
                         coverage_file))
      (retcode, output) = proc.RunCommandFull(convert_command,
                                              collect_output=True)
      if output != 0:
        logging.error('Conversion to LCOV failed. Exiting.')
        sys.exit(1)
      lcov_file = coverage_file + '.lcov'
      logging.info('Conversion to lcov complete')
    shutil.copy(lcov_file, upload_path)
    logging.info('LCOV file uploaded to %s' % (upload_path))


def main():
  # Command line parsing
  parser = optparse.OptionParser()
  # Path where the .coverage to .lcov converter tools are stored.
  parser.add_option('-t',
                    '--tools_path',
                    dest='tools_path',
                    default=None,
                    help='Location of the coverage tools (windows only)')
  parser.add_option('-u',
                    '--upload',
                    dest='upload_path',
                    default=None,
                    help='Location where the results should be uploaded')
  # We need the revision number so that we can generate the output file of the
  # format chrome_<platform>_<revision>.lcov
  parser.add_option('-r',
                    '--revision',
                    dest='revision',
                    default=None,
                    help='Revision number of the Chromium source repo')
  # Root of the source tree. Needed for converting the generated .coverage file
  # on Windows to the open source lcov format.
  parser.add_option('-s',
                    '--src_root',
                    dest='src_root',
                    default=None,
                    help='Root of the source repository')

  (options, args) = parser.parse_args()
  
  if options.revision == None:
    parser.error('Revision number not specified')
  if options.src_root == None:
    parser.error('Source root not specified')
  if options.upload_path == None:
    parser.error('Upload path not specified')
   
  if IsWindows():
    # Initialize coverage
    cov = Coverage(options.revision,
                   options.src_root,
                   options.tools_path)
    # Instrument the binaries
    coverage_file = cov.SetUp(windows_binaries)
    if coverage_file != None:
      # Run all the tests
      for test in windows_tests:
        test = os.path.join(options.src_root, 'chrome', 'Release', test)
        logging.info('Executing test %s: ' % test)
        (retcode, output) = proc.RunCommandFull(test, collect_output=True)
        if retcode != 0: # Die if the tests fail
          logging.error('One or more tests failed in %s. Exiting.' % test)
          sys.exit(retcode)
    else: 
      logging.error('Error during instrumentation.')
      sys.exit(1)

    cov.Upload(coverage_file,
               options.upload_path,
               os.path.join(options.src_root, 'chrome', 'Release'),
               options.src_root)
    cov.TearDown()


if __name__ == '__main__':
  sys.exit(main())
  
