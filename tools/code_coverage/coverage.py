#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Module to setup and generate code coverage data
"""

import optparse
import os
import shutil
import sys
import tempfile

import google.process_utils as proc

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
               vsts_path = None,
               lcov_converter_path = None):
    self.revision = revision
    self.instrumented = False
    self.vsts_path = vsts_path
    self.lcov_converter_path = lcov_converter_path
  
  
  def _IsWindows(self):
    """Checks if the current platform is Windows.
    """
    return sys.platform[:3] == 'win':
  
  
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
      return None
    coverage_file = None
    if self._IsWindows():
      # TODO(niranjan): Add a check that to verify that the binaries were built
      # using the /PROFILE linker flag.
      if self.vsts_path == None or self.lcov_converter_path == None:
        return None
      # Remove trailing slashes
      self.vsts_path = self.vsts_path.rstrip('\\') 
      instrument_command = '%s /COVERAGE ' % (os.path.join(self.vsts_path,
                                                           'vsinstr.exe'))
      for binary in binaries:
        print 'binary = %s' % binary
        print 'instrument_command = %s' % instrument_command
        # Instrument each binary in the list
        (retcode, output) = proc.RunCommandFull(instrument_command + binary,
                                                collect_output=True)
        print output
        # Check if the file has been instrumented correctly.
        if output.pop().rfind('Successfully instrumented') == -1:
          # TODO(niranjan): Change print to logging.
          print 'Error instrumenting %s' % binary
          return None

      # Generate the file name for the coverage results
      self._dir = tempfile.mkdtemp()
      coverage_file = os.path.join(self._dir, 'chrome_win32_%s.coverage' % 
                                              (self.revision))

      # After all the binaries have been instrumented, we start the counters.
      counters_command = ('%s -start:coverage -output:%s' %
                          (os.path.join(self.vsts_path, 'vsperfcmd.exe'),
                          coverage_file))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      print output
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
    
    if self._IsWindows():
      # Stop counters
      counters_command = ('%s -start:coverage -shutdown' % 
                         (os.path.join(self.vsts_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      
      # TODO(niranjan): Revert the instrumented binaries to their original
      # versions.
    else:
      return
    # Delete all the temp files and folders
    if self._dir != None:
      os.removedirs(self._dir)
    # Reset the instrumented flag.
    self.instrumented = False
    

  def Upload(self, coverage_file, upload_path, sym_path=None, src_root=None):
    """Upload the results to the dashboard.

    This method uploads the coverage data to a dashboard where it will be
    processed. On Windows, this method will first convert the .coverage file to
    the lcov format.

    Args:
      coverage_file: The coverage data file to upload.
      upload_path: Destination where the coverage data will be processed.
      sym_path: Symbol path for the build (Win32 only)
      src_root: Root folder of the source tree (Win32 only)
    
    Returns:
      True on success.
      False on failure.
    """
    if self._IsWindows():
      # Stop counters
      counters_command = ('%s -start:coverage -shutdown' % 
                         (os.path.join(self.vsts_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      # Convert the .coverage file to lcov format
      if self.lcov_converter_path == False: 
        return False
      self.lcov_converter_path = self.lcov_converter_path.rstrip('\\')
      convert_command = ('%s -sym_path=%s -src_root=%s ' % 
                         os.path.join(self.lcov_converter_path, 
                                      'coverage_analyzer.exe'),
                         sym_path,
                         src_root)
      (retcode, output) = proc.RunCommandFull(convert_command + coverage_file,
                                              collect_output=True)
      shutil.copy(coverage_file, coverage_file.replace('.coverage', ''))
    # TODO(niranjan): Upload this somewhere!


