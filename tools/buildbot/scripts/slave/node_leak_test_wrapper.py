#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to run node leak tests from a saved cache.

  This tool will launch the test shell from a previously recorded cache.

  When this is run, the current directory (cwd) should be the outer build
  directory (e.g., webkit-release/build/).

  For a list of command-line options, call this script with '--help'.
"""

import optparse
import os
import random
import sys

import chromium_utils
import slave_utils

def main(options, args):
  """Parse options and call run_node_leak_test.py.
  """

  build_dir = os.path.abspath(options.build_dir)

  leak_tests_dir = chromium_utils.FindUpward(build_dir, 'webkit', 'tools',
                                           'leak_tests')

  run_node_leak_test = os.path.join(leak_tests_dir, 'run_node_leak_test.py')

  cache_zip_path =  chromium_utils.FindUpward(build_dir, 'data', 'saved_caches',
                                            'leak_tests', options.cache_file)

  staging_dir = slave_utils.GetStagingDir(build_dir)
  cache_dir = os.path.join(staging_dir, options.target, 'cache')
  chromium_utils.RemoveDirectory(cache_dir)
  chromium_utils.ExtractZip(cache_zip_path, cache_dir)

  command = [sys.executable, run_node_leak_test, '--cache-dir', cache_dir,
             '--seed', str(random.randint(0, sys.maxint))]

  if options.target:
    command.extend(['--target', options.target])

  if options.url_list:
    command.extend(['--url-list', options.url_list])

  if options.time_out_ms:
    command.extend(['--time-out-ms', str(options.time_out_ms)])

  command.extend(args)

  return chromium_utils.RunCommand(command)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--target', default='Debug',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--build-dir', default='webkit',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')
  option_parser.add_option('', '--cache-file', default='cache.zip',
                           help='previously recorded cache file to play back '
                                'from, relative to webkit/data/leak_tests, '
                                'should be a ZIP archive')
  option_parser.add_option('', '--url-list', default='',
                           help='URL input file, with leak expectations')
  option_parser.add_option('', '--time-out-ms', default=30000,
                           help='time out for each test')

  options, args = option_parser.parse_args()
  sys.exit(main(options, args))
