# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Some utility methods for getting paths used by run_webkit_tests.py.
"""

import errno
import os
import subprocess
import sys

import google.path_utils


class PathNotFound(Exception): pass

# Save some paths here so we don't keep re-evaling.
_webkit_root = None
_layout_data_dir = None
_expected_results_dir = None
_platform_results_dirs = {}

# TODO this should probably be moved into path_utils as ToUnixPath().
def WinPathToUnix(path):
  """Convert a windows path to use unix-style path separators (a/b/c)."""
  return path.replace('\\', '/')

def WebKitRoot():
  """Returns the full path to the directory containing webkit.sln.  Raises
  PathNotFound if we're unable to find webkit.sln."""
  global _webkit_root
  if _webkit_root:
    return _webkit_root
  webkit_sln_path = google.path_utils.FindUpward(google.path_utils.ScriptDir(),
                                                 'webkit.sln')
  _webkit_root = os.path.dirname(webkit_sln_path)
  return _webkit_root

def LayoutDataDir():
  """Gets the full path to the tests directory.  Raises PathNotFound if
  we're unable to find it."""
  global _layout_data_dir
  if _layout_data_dir:
    return _layout_data_dir
  _layout_data_dir = google.path_utils.FindUpward(WebKitRoot(), 'webkit',
                                                  'data', 'layout_tests')
  return _layout_data_dir

def ExpectedResultsDir():
  """Gets the full path to the custom_results directory.  Raises
  PathNotFound if we're unable to find it."""
  global _expected_results_dir
  if _expected_results_dir:
    return _expected_results_dir
  _expected_results_dir = google.path_utils.FindUpward(WebKitRoot(), 'webkit',
                                                       'data',
                                                       'layout_test_results')
  return _expected_results_dir

def CustomExpectedResultsDir(custom_id):
  """Gets the full path to the directory in which custom expected results for
  this app and build type are located.

  Args:
    custom_id: a string specifying the particular set of results to use (e.g.,
      'v8' or 'kjs')
  """
  return os.path.join(ExpectedResultsDir(), custom_id)

def PlatformResultsDir(name):
  """Gets the full path to a platform-specific results directory.  Raises
  PathNotFound if we're unable to find it."""
  global _platform_results_dirs
  if _platform_results_dirs.get(name):
    return _platform_results_dirs[name]
  _platform_results_dirs[name] = google.path_utils.FindUpward(WebKitRoot(),
      'webkit', 'data', 'layout_tests', 'LayoutTests', 'platform', name)
  return _platform_results_dirs[name]

def ExpectedFilename(filename, suffix, custom_result_id):
  """Given a test name, returns an absolute filename to the most specific
  applicable file of expected results.

  Args:
    filename: absolute filename to test file
    suffix: file suffix of the expected results, including dot; e.g. '.txt'
        or '.png'.  This should not be None, but may be an empty string.
    custom_result_id: Tells us where to look for custom results.  Currently
        this is either kjs or v8.

  Return:
    If a file named <testname>-expected<suffix> exists in the subdirectory
    of the ExpectedResultsDir() specified by this platform's identifier,
    return its absolute path.  Otherwise, return a path to a
    <testname>-expected<suffix> file under the MacExpectedResultsDir() or
    <testname>-expected<suffix> file under the MacLeopardExpectedResultsDir()
    or (if not found there) in the same directory as the test file (even if
    that default file does not exist).
  """
  testname = os.path.splitext(RelativeTestFilename(filename))[0]
  results_filename = testname + '-expected' + suffix
  results_dirs = [
    CustomExpectedResultsDir(custom_result_id),
    CustomExpectedResultsDir('common'),
    LayoutDataDir()
  ]

  for results_dir in results_dirs:
    platform_file = os.path.join(results_dir, results_filename)
    if os.path.exists(platform_file):
      return platform_file

  # for 'base' tests, we need to look for mac-specific results
  if testname.startswith('LayoutTests'):
    layout_test_results_dirs = [
      PlatformResultsDir('mac'),
      PlatformResultsDir('mac-leopard'),
      PlatformResultsDir('mac-tiger')
    ]
    rel_testname = testname[len('LayoutTests') + 1:]
    rel_filename = rel_testname + '-expected' + suffix
    for results_dir in layout_test_results_dirs:
      platform_file = os.path.join(results_dir, rel_filename)
      if os.path.exists(platform_file):
        return platform_file

  # Failed to find the results anywhere, return default path anyway
  return os.path.join(results_dirs[0], results_filename)

def TestShellBinary():
  """Returns the name of the test_shell executable."""
  return 'test_shell.exe'

def TestShellBinaryPath(target):
  """Gets the full path to the test_shell binary for the target build
  configuration. Raises PathNotFound if the file doesn't exist"""
  full_path = os.path.join(WebKitRoot(), target, TestShellBinary())
  if not os.path.exists(full_path):
    # try chrome's output directory in case test_shell was built by chrome.sln
    full_path = google.path_utils.FindUpward(WebKitRoot(), 'chrome', target,
                                             TestShellBinary())
    if not os.path.exists(full_path):
      raise PathNotFound('unable to find test_shell at %s' % full_path)
  return full_path

def RelativeTestFilename(filename):
  """Provide the filename of the test relative to the layout data
  directory as a unix style path (a/b/c)."""
  return WinPathToUnix(filename[len(LayoutDataDir()) + 1:])

# Map platform specific path utility functions.  We do this as a convenience
# so importing path_utils will get all path related functions even if they are
# platform specific.
def GetAbsolutePath(path):
  # Avoid circular import by delaying it.
  import layout_package.platform_utils
  platform_util = layout_package.platform_utils.PlatformUtility(WebKitRoot())
  return platform_util.GetAbsolutePath(path)

def FilenameToUri(path):
  # Avoid circular import by delaying it.
  import layout_package.platform_utils
  platform_util = layout_package.platform_utils.PlatformUtility(WebKitRoot())
  return platform_util.FilenameToUri(path)

