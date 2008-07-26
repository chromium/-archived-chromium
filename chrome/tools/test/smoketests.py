#!/bin/env python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
Runs all the available unit tests, layout tests, page-cycler tests, etc.
for a build of Chrome, imitating a buildbot.

Usage examples:
  smoketests.py
  smoketests.py --target=debug --build-type=kjs
  smoketests.py --nopage-cycler
  smoketests.py --tests=unit,ui --verbose

For a full list of options, pass the '--help' switch.

[Alternatively, this script will kill all the tests' executables, in case one
got orphaned during a previous run of this script.  (This generally only
happens during script development.)]

"""

import errno
import optparse
import os
import subprocess
import sys
import time

import google.httpd_utils
import google.path_utils
import google.process_utils


# Keep a global httpd object so it can be killed in the event of errors.
_httpd = None

# All the available commands, by name. Items in the command-line lists may
# contain various keywords, listed in the "Substitutions" section below.
# The target build directory will be prepended to the first item in each
# command list.
COMMANDS = {'ipc':               ['ipc_tests.exe'],
            'unit':              ['unit_tests.exe'],
            'ui':                ['ui_tests.exe', '%(page_heap)s'],
            'ui-single':         ['ui_tests.exe', '--single-process'],
            'test_shell':        ['test_shell_tests.exe'],
            'page-cycler-moz':   ['page_cycler_tests.exe',
                                  '--gtest_filter=PageCycler*.MozFile'],
            'page-cycler-moz-http': ['page_cycler_tests.exe',
                                     '--gtest_filter=PageCycler*.MozHttp'],
            'page-cycler-intl1': ['page_cycler_tests.exe',
                                  '--gtest_filter=PageCycler*.Intl1File'],
            'page-cycler-intl2': ['page_cycler_tests.exe',
                                  '--gtest_filter=PageCycler*.Intl2File'],
            'page-cycler-bloat-http': ['page_cycler_tests.exe',
                                       '--gtest_filter=PageCycler*.BloatHttp'],
            'startup':           ['startup_tests.exe',
                                  '--gtest_filter=Startup*.*'],
            'dest-startup':      ['startup_tests.exe',
                                  '--gtest_filter=DestinationsStartupTest.*'],
            'selenium':          ['selenium_tests.exe'],
            'plugin':            ['plugin_tests.exe'],
            'installer':         ['installer_unittests.exe'],
            'webkit':            ['%(python)s',
                                  '%(slave_scripts)s/layout_test_wrapper.py',
                                  '--build-type', '%(build_type)s',
                                  '--target', '%(target)s',
                                  '%(page_heap)s'],
           }

# Certain tests are not run for each build type.
SKIPPED = {'Release': ['plugin'],
           'Debug':   ['selenium', 'webkit']}

def _BuildbotScriptPath(sub_dir):
  """Returns the full path to the given subdir of tools/buildbot/scripts."""
  this_script_dir = google.path_utils.ScriptDir()
  return google.path_utils.FindUpward(this_script_dir, 'tools', 'buildbot',
                                      'scripts', sub_dir)


def _MakeSubstitutions(list, options):
  """Makes substitutions in each item of a list and returns the resulting list.

  Args:
    list: a list of strings, optionally containing certain %()s substitution
        tags listed below
    options: options as returned by optparse
  """
  this_script_dir = google.path_utils.ScriptDir()
  python_path = google.path_utils.FindUpward(this_script_dir,
                                             'third_party',
                                             'python_24',
                                             'python_slave.exe')

  substitutions = {'target':        options.target,
                   'build_type':    options.build_type,
                   'page_heap':     '',
                   'python':        python_path,
                   'slave_scripts': _BuildbotScriptPath('slave'),
                  }
  if options.build_type == 'kjs':
    substitutions['page_heap'] = '--enable-pageheap'
  return [word % substitutions for word in list]


def main(options, args):
  """Runs all the selected tests for the given build type and target."""
  options.build_type = options.build_type.lower()
  options.target = options.target.title()

  this_script_dir = google.path_utils.ScriptDir()
  test_path = google.path_utils.FindUpward(this_script_dir,
                                           'chrome', options.target)

  # Add the buildbot script paths to the module search path.
  sys.path.insert(0, _BuildbotScriptPath('slave'))
  sys.path.insert(0, _BuildbotScriptPath('common'))

  # Collect list of tests to run.
  if options.tests == '':
    tests = sorted(COMMANDS.keys())
  else:
    tests = set()
    requested_tests = options.tests.lower().split(',')
    for test in requested_tests:
      if test in COMMANDS:
        tests.add(test)
      else:
        print 'Ignoring unknown test "%s"' % test

  # Check page-cycler data, since the tests choke if it isn't available.
  try:
    page_cycler_data = google.path_utils.FindUpward(this_script_dir,
                                                    'data',
                                                    'page_cycler')
  except google.path_utils.PathNotFound:
    # Were we going to run any page-cycler tests?
    if (not options.nopage_cycler and
        len([x for x in tests if x.startswith('page-cycler')])):
      print 'Skipping page-cycler tests (no data)'
    options.nopage_cycler = True
  
  # Start an httpd if needed.
  http_tests = [x for x in tests if x.endswith('-http')]
  if http_tests and not options.nopage_cycler and not options.nohttp:
    try:
      _httpd = google.httpd_utils.StartServer(document_root=page_cycler_data)
    except google.httpd_utils.HttpdNotStarted:
      print 'Skipping http tests (httpd failed to start)'
      options.nohttp = True

  # Remove tests not desired.
  if options.nopage_cycler:
    tests = [x for x in tests if not x.startswith('page-cycler')]
  if options.nowebkit and 'webkit' in tests:
    tests.remove('webkit')
  if options.nohttp:
    tests = [x for x in tests if not x.endswith('-http')]

  # Remove tests skipped for this build target.
  for skip in SKIPPED[options.target]:
    if skip in tests:
      print 'Skipping %s for %s build' % (skip, options.target)
      tests.remove(skip)

  if not len(tests):
    print 'No tests to run.'
    return 0

  # Run each test, substituting strings as needed.
  failures = []
  start_time = time.time()
  for test in tests:
    test_start_time = time.time()
    command = _MakeSubstitutions(COMMANDS[test], options)
    command[0] = os.path.join(test_path, command[0])
    if options.verbose:
      print
    print 'Running %s:' % test,
    try:
      result = google.process_utils.RunCommand(command, options.verbose)
    except google.process_utils.CommandNotFound:
      print '%s' % e
    if options.verbose:
      print test,
    print '(%ds)' % (time.time() - test_start_time),
    if result:
      print 'FAIL'
      failures.append(test)
    else:
      print 'PASS'

  print 'Total time: %ds' % (time.time() - start_time)
  if len(failures):
    print 'Failed tests:'
    print os.linesep.join(failures)
  else:
    print 'All tests passed. Hurrah!'

  return len(failures)

if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--build-type', default='v8',
                           help='build type (V8 or KJS), used by webkit tests')
  option_parser.add_option('', '--verbose', action='store_true', default=False,
                           help='show full output from every command')
  option_parser.add_option('', '--nopage-cycler', action='store_true',
                           default=False, help='disable page-cycler tests')
  option_parser.add_option('', '--nowebkit', action='store_true',
                           default=False, help='disable webkit (layout) tests')
  option_parser.add_option('', '--nohttp', action='store_true',
                           default=False,
                           help="don't run tests (e.g. page_cycler) with http")
  option_parser.add_option('', '--tests', default='',
                           help='comma-separated list of tests to run, from '
                                '{%s}' % ', '.join(sorted(COMMANDS.keys())))
  option_parser.add_option('', '--killall', action='store_true', default=False,
                           help='kill all test executables (and run no tests)')
  options, args = option_parser.parse_args()

  if options.killall:
    kill_list = _MakeSubstitutions([COMMANDS[x][0] for x in COMMANDS.keys()],
                                   options)
    kill_list = set([os.path.basename(x) for x in kill_list])
    sys.exit(google.process_utils.KillAll(kill_list))

  try:
    result = main(options, args)
  finally:
    # Kill the httpd.
    if _httpd:
      _httpd.StopServer(force=True)
  sys.exit(result)
