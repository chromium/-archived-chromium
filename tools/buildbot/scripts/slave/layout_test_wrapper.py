#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper script to run layout tests on the buildbots.

Runs the run_webkit_tests.py script found in webkit/tools/layout_tests above
this script. For a complete list of command-line options, pass '--help' on the
command line.

To pass additional options to run_webkit_tests without having them interpreted
as options for this script, include them in an '--options="..."' argument. In
addition, a list of one or more tests or test directories, specified relative
to the main webkit test directory, may be passed on the command line.
"""

import optparse
import os
import sys

import chromium_utils
import slave_utils


def main(options, args):
  """Parse options and call run_webkit_tests.py, using Python from the tree."""
  build_dir = os.path.abspath(options.build_dir)

  # Disable the page heap in case it got left enabled by some previous process.
  try:
    slave_utils.SetPageHeap(build_dir, 'test_shell.exe', False)
  except chromium_utils.PathNotFound:
    # If we don't have gflags.exe, report it but don't worry about it.
    print 'Warning: Couldn\'t disable page heap, if it was already enabled.'
    pass

  webkit_tests_dir = chromium_utils.FindUpward(build_dir,
                                             'webkit', 'tools', 'layout_tests')
  run_webkit_tests = os.path.join(webkit_tests_dir, 'run_webkit_tests.py')
  python_exe = chromium_utils.FindUpward(build_dir,
                                       'third_party',
                                       'python_24',
                                       'python.exe')

  setup_mount = chromium_utils.FindUpward(build_dir,
                                       'third_party',
                                       'cygwin',
                                       'setup_mount.bat')

  chromium_utils.RunCommand([setup_mount])

  command = [python_exe,
             run_webkit_tests,
             '--noshow-results',
             '--verbose'  # Verbose output is enabled to support the dashboard.
            ]

  if options.results_directory:
    # run_webkit_tests expects the results directory to be either an absolute
    # path (starting with '/') or relative to Debug or Release.  We expect it
    # to be relative to the build_dir, which is a parent of Debug or Release.
    # Thus if it's a relative path, we need to move it out one level.
    if not options.results_directory.startswith('/'):
      options.results_directory = os.path.join('..', options.results_directory)
    chromium_utils.RemoveDirectory(options.results_directory)
    command.extend(['--results-directory', options.results_directory])

  if options.target:
    command.extend(['--target', options.target])
    if options.target == 'Debug':
      command.append('--time-out-ms=20000')

  if options.build_type:
    command.extend(['--build-type', options.build_type])

  if options.pixel_tests:
    command.append('--pixel-tests')

  if options.enable_pageheap:
    # If we're actually requesting pageheap checking, complain if we don't have
    # gflags.exe.
    slave_utils.SetPageHeap(build_dir, 'test_shell.exe', True)
    command.append('--time-out-ms=120000')

  # The list of tests is given as arguments.
  command.extend(options.options.split(' '))
  command.extend(args)
  result = chromium_utils.RunCommand(command)

  if options.enable_pageheap:
    slave_utils.SetPageHeap(build_dir, 'test_shell.exe', False)

  return result

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('-o', '--results-directory', default='',
                           help='output results directory')
  option_parser.add_option('', '--build-dir', default='webkit',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')
  option_parser.add_option('', '--target', default='',
      help='test_shell build configuration (Release or Debug)')
  option_parser.add_option('', '--build-type', default='',
      help='build identifier, for custom results and test lists (v8 or kjs)')
  option_parser.add_option('', '--options', default='',
      help='additional options to pass to run_webkit_tests.py')
  option_parser.add_option('', '--pixel-tests', action='store_true',
                           default=False,
                           help='enable pixel-to-pixel PNG comparisons')
  option_parser.add_option('', '--enable-pageheap', action='store_true',
                           default=False, help='Enable page heap checking')
  options, args = option_parser.parse_args()
  sys.exit(main(options, args))

