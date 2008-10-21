#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to run playback tests from a saved cache.

  This tool will launch the test shell from a previously recorded cache and
  event log, using the saved mouse and keyboard events in the event log to
  drive the shell and ultimately close it.  The stats gathered from these
  actions will then be dumped in the output.

  When this is run, the current directory (cwd) should be the outer build
  directory (e.g., webkit-release/build/).

  For a list of command-line options, call this script with '--help'.
"""

import optparse
import os
import sys

import chromium_utils
import slave_utils

def main(options, args):
  def RunTestShell(test_shell_path, iterations):
    exit_code = 0
    for i in range(0, iterations):
      command = [test_shell_path, '--playback-mode', '--stats',
                 '--cache-dir=%s' % cache_dir]
      exit_code = chromium_utils.RunCommand(command)
      if exit_code != 0:
        return exit_code
    return exit_code

  build_dir = os.path.abspath(options.build_dir)
  test_shell_path = os.path.join(build_dir, options.target, 'test_shell.exe')
  if not os.path.exists(test_shell_path):
    raise chromium_utils.PathNotFound('Unable to find %s' % test_shell_path)

  ref_test_shell_path = chromium_utils.FindUpward(build_dir, 'tools', 'test',
                                                'reference_build',
                                                'webkit-release',
                                                'test_shell.exe')

  cache_zip_path =  chromium_utils.FindUpward(build_dir, 'data', 'saved_caches',
                                            'playback_tests',
                                            options.cache_file)

  staging_dir = slave_utils.GetStagingDir(build_dir)
  cache_dir = os.path.join(staging_dir, options.target, 'cache')
  chromium_utils.RemoveDirectory(cache_dir)
  chromium_utils.ExtractZip(cache_zip_path, cache_dir)

  iterations = int(options.iterations)

  reference_exit_code = 0
  latest_exit_code = 0

  print
  print '=REFERENCE='
  reference_exit_code = RunTestShell(ref_test_shell_path, iterations)

  print
  print '=LATEST='
  latest_exit_code = RunTestShell(test_shell_path, iterations)

  # If the latest test shell had errors, report its error code.  Otherwise,
  # if the reference test shell had errors, report its error code.  Otherwise
  # report zero for success.
  if latest_exit_code != 0:
    return latest_exit_code
  elif reference_exit_code != 0:
    return reference_exit_code
  return 0

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--build-dir', default='webkit',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')
  option_parser.add_option('', '--cache-file', default='cache.zip',
                           help='previously recorded cache file to play back '
                                'from, relative to '
                                'webkit/data/playback_tests; should be a ZIP '
                                'archive')
  option_parser.add_option('', '--iterations', default='3',
                           help='number of times to run each test_shell: '
                                'more runs means a more stable result, but '
                                'will also take longer')

  options, args = option_parser.parse_args()
  sys.exit(main(options, args))
