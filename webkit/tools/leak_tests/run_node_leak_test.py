#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run node leak tests using the test_shell.

TODO(pjohnson): Add a way for layout tests (and other local files in the
working copy) to be easily run by specifying paths relative to webkit (or
something similar).
"""

import logging
import optparse
import os
import random
import re
import sys

import google.logging_utils
import google.path_utils
import google.platform_utils
import google.process_utils

# Magic exit code to indicate a new fix.
REBASELINE_EXIT_CODE = -88

# Status codes.
PASS, FAIL, REBASELINE = range(3)

# The test list files are found in this subdirectory, which must be a sibling
# to this script itself.
TEST_FILE_DIR = 'test_lists'

# TODO(pjohnson): Find a way to avoid this duplicate code.  This function has
# been shamelessly taken from layout_tests/layout_package.
_webkit_root = None

def WebKitRoot():
  """Returns the full path to the directory containing webkit.sln.  Raises
  PathNotFound if we're unable to find webkit.sln.
  """

  global _webkit_root
  if _webkit_root:
    return _webkit_root
  webkit_sln_path = google.path_utils.FindUpward(google.path_utils.ScriptDir(),
                                                 'webkit.sln')
  _webkit_root = os.path.dirname(webkit_sln_path)
  return _webkit_root

def GetAbsolutePath(path):
  platform_util = google.platform_utils.PlatformUtility(WebKitRoot())
  return platform_util.GetAbsolutePath(path)

# TODO(pjohnson): Find a way to avoid this duplicated code.  This function has
# been mostly copied from another function, TestShellBinary, in
# layout_tests/layout_package.
def TestShellTestBinary(target):
  """Gets the full path to the test_shell_tests binary for the target build
  configuration.  Raises PathNotFound if the file doesn't exist.
  """

  full_path = os.path.join(WebKitRoot(), target, 'test_shell_tests.exe')
  if not os.path.exists(full_path):
    # Try chrome's output directory in case test_shell was built by chrome.sln.
    full_path = google.path_utils.FindUpward(WebKitRoot(), 'chrome', target,
                                             'test_shell_tests.exe')
    if not os.path.exists(full_path):
      raise PathNotFound('unable to find test_shell_tests at %s' % full_path)
  return full_path

class NodeLeakTestRunner:
  """A class for managing running a series of node leak tests.
  """

  def __init__(self, options, urls):
    """Collect a list of URLs to test.

    Args:
      options: a dictionary of command line options
      urls: a list of URLs in the format:
            (url, expected_node_leaks, expected_js_leaks) tuples
    """

    self._options = options
    self._urls = urls

    self._test_shell_test_binary = TestShellTestBinary(options.target)

    self._node_leak_matcher = re.compile('LEAK: (\d+) Node')
    self._js_leak_matcher = re.compile('Leak (\d+) JS wrappers')

  def RunCommand(self, command):
    def FindMatch(line, matcher, group_number):
      match = matcher.match(line)
      if match:
        return int(match.group(group_number))
      return 0

    (code, output) = google.process_utils.RunCommandFull(command, verbose=True,
                                                         collect_output=True,
                                                         print_output=False)
    node_leaks = 0
    js_leaks = 0

    # Print a row of dashes.
    if code != 0:
      print '-' * 75
      print 'OUTPUT'
      print

    for line in output:
      # Sometimes multiple leak lines are printed out, which is why we
      # accumulate them here.
      node_leaks += FindMatch(line, self._node_leak_matcher, 1)
      js_leaks += FindMatch(line, self._js_leak_matcher, 1)

      # If the code indicates there was an error, print the output to help
      # figure out what happened.
      if code != 0:
        print line

    # Print a row of dashes.
    if code != 0:
      print '-' * 75
      print

    return (code, node_leaks, js_leaks)

  def RunUrl(self, test_url, expected_node_leaks, expected_js_leaks):
    shell_args = ['--gtest_filter=NodeLeakTest.*TestURL',
                  '--time-out-ms=' + str(self._options.time_out_ms),
                  '--test-url=' + test_url,
                  '--playback-mode']

    if self._options.cache_dir != '':
      shell_args.append('--cache-dir=' + self._options.cache_dir)

    command = [self._test_shell_test_binary] + shell_args
    (exit_code, actual_node_leaks, actual_js_leaks) = self.RunCommand(command)

    logging.info('%s\n' % test_url)

    if exit_code != 0:
      # There was a crash, or something else went wrong, so duck out early.
      logging.error('Test returned: %d\n' % exit_code)
      return FAIL

    result = ('TEST RESULT\n'
              '    Node Leaks: %d (actual), %d (expected)\n'
              '    JS Leaks: %d (actual), %d (expected)\n' %
              (actual_node_leaks, expected_node_leaks,
               actual_js_leaks, expected_js_leaks))

    success = (actual_node_leaks <= expected_node_leaks and
               actual_js_leaks <= expected_js_leaks)

    if success:
      logging.info(result)
    else:
      logging.error(result)
      logging.error('Unexpected leaks found!\n')
      return FAIL

    if (expected_node_leaks > actual_node_leaks or
        expected_js_leaks > actual_js_leaks):
       logging.warn('Expectations may need to be re-baselined.\n')
       # TODO(pjohnson): Return REBASELINE here once bug 1177263 is fixed and
       # the expectations have been lowered again.

    return PASS

  def Run(self):
    status = PASS
    results = [0, 0, 0]
    failed_urls = []
    rebaseline_urls = []

    for (test_url, expected_node_leaks, expected_js_leaks) in self._urls:
      result = self.RunUrl(test_url, expected_node_leaks, expected_js_leaks)
      if result == PASS:
        results[PASS] += 1
      elif result == FAIL:
        results[FAIL] += 1
        failed_urls.append(test_url)
        status = FAIL
      elif result == REBASELINE:
        results[REBASELINE] += 1
        rebaseline_urls.append(test_url)
        if status != FAIL:
          status = REBASELINE
    return (status, results, failed_urls, rebaseline_urls)

def main(options, args):
  if options.seed != None:
    random.seed(options.seed)

  # Set up logging so any messages below logging.WARNING are sent to stdout,
  # otherwise they are sent to stderr.
  google.logging_utils.config_root(level=logging.INFO,
                                   threshold=logging.WARNING)

  if options.url_list == '':
    logging.error('URL test list required')
    sys.exit(1)

  url_list = os.path.join(os.path.dirname(sys.argv[0]), TEST_FILE_DIR,
                          options.url_list)
  url_list = GetAbsolutePath(url_list);

  lines = []
  file = None
  try:
    file = open(url_list, 'r')
    lines = file.readlines()
  finally:
    if file != None:
      file.close()

  expected_matcher = re.compile('(\d+)\s*,\s*(\d+)')

  urls = []
  for line in lines:
    line = line.strip()
    if len(line) == 0 or line.startswith('#'):
      continue
    list = line.rsplit('=', 1)
    if len(list) < 2:
      logging.error('Line "%s" is not formatted correctly' % line)
      continue
    match = expected_matcher.match(list[1].strip())
    if not match:
      logging.error('Line "%s" is not formatted correctly' % line)
      continue
    urls.append((list[0].strip(), int(match.group(1)), int(match.group(2))))

  random.shuffle(urls)
  runner = NodeLeakTestRunner(options, urls)
  (status, results, failed_urls, rebaseline_urls) = runner.Run()

  logging.info('SUMMARY\n'
               '    %d passed\n'
               '    %d failed\n'
               '    %d re-baseline\n' %
               (results[0], results[1], results[2]))

  if len(failed_urls) > 0:
    failed_string = '\n'.join('    ' + url for url in failed_urls)
    logging.error('FAILED URLs\n%s\n' % failed_string)

  if len(rebaseline_urls) > 0:
    rebaseline_string = '\n'.join('    ' + url for url in rebaseline_urls)
    logging.warn('RE-BASELINE URLs\n%s\n' % rebaseline_string)

  if status == FAIL:
    return 1
  elif status == REBASELINE:
    return REBASELINE_EXIT_CODE
  return 0

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--target', default='Debug',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--cache-dir', default='',
                           help='use a specified cache directory')
  option_parser.add_option('', '--url-list', default='',
                           help='URL input file, with leak expectations, '
                                 'relative to webkit/tools/leak_tests')
  option_parser.add_option('', '--time-out-ms', default=30000,
                           help='time out for each test')
  option_parser.add_option('', '--seed', default=None,
                           help='seed for random number generator, use to '
                                'reproduce the exact same order for a '
                                'specific run')
  options, args = option_parser.parse_args()
  sys.exit(main(options, args))

