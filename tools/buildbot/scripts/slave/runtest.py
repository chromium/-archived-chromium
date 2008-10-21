#!/usr/bin/python
# Copyright (c) 2007-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to run a chrome test executable, used by the buildbot slaves.

  When this is run, the current directory (cwd) should be the outer build
  directory (e.g., chrome-release/build/).

  For a list of command-line options, call this script with '--help'.
"""

import logging
import optparse
import os
import sys

import chromium_utils
import slave_utils

# So we can import google.*_utils below with native Pythons.
sys.path.append(os.path.abspath('src/tools/python'))

import google.httpd_utils
import google.platform_utils


USAGE = '%s [options] test.exe [test args]' % os.path.basename(sys.argv[0])


def main_mac(options, args):
  if len(args) < 1:
    raise chromium_utils.MissingArgument('Usage: %s' % USAGE)

  test_exe = args[0]
  build_dir = os.path.normpath(os.path.abspath(options.build_dir))
  test_exe_path = os.path.join(build_dir, options.target, test_exe)
  if not os.path.exists(test_exe_path):
    pre = 'Unable to find %s\n' % test_exe_path
    build_dir = os.path.join(options.build_dir, '..', 'xcodebuild')
    build_dir = os.path.normpath(os.path.abspath(build_dir))
    test_exe_path = os.path.join(build_dir, options.target, test_exe)
    if not os.path.exists(test_exe_path):
      msg = pre + 'Unable to find %s' % test_exe_path
      raise chromium_utils.PathNotFound(msg)

  command = [test_exe_path]
  command.extend(args[1:])
  result = chromium_utils.RunCommand(command)

  return result

def main_linux(options, args):
  if len(args) < 1:
    raise chromium_utils.MissingArgument('Usage: %s' % USAGE)

  test_exe = args[0]
  build_dir = os.path.normpath(os.path.abspath(options.build_dir))
  test_exe_path = os.path.join(build_dir, options.target, test_exe)
  if not os.path.exists(test_exe_path):
    raise chromium_utils.PathNotFound('Unable to find %s' % test_exe_path)

  command = [test_exe_path]
  command.extend(args[1:])
  result = chromium_utils.RunCommand(command)

  return result

def main_win(options, args):
  """Using the target build configuration, run the executable given in the
  first non-option argument, passing any following arguments to that
  executable.
  """
  if len(args) < 1:
    raise chromium_utils.MissingArgument('Usage: %s' % USAGE)

  test_exe = args[0]
  build_dir = os.path.abspath(options.build_dir)
  test_exe_path = os.path.join(build_dir, options.target, test_exe)
  if not os.path.exists(test_exe_path):
    raise chromium_utils.PathNotFound('Unable to find %s' % test_exe_path)

  http_server = None
  if options.document_root:
    platform_util = google.platform_utils.PlatformUtility(build_dir)

    # Name the output directory for the exe, without its path or suffix.
    # e.g., chrome-release/httpd_logs/unit_tests/
    test_exe_name = os.path.basename(test_exe_path).rsplit('.', 1)[0]
    output_dir = os.path.join(slave_utils.SlaveBaseDir(build_dir),
                              'httpd_logs',
                              test_exe_name)

    apache_config_dir = google.httpd_utils.ApacheConfigDir(build_dir)
    httpd_conf_path = os.path.join(apache_config_dir, 'httpd.conf')
    mime_types_path = os.path.join(apache_config_dir, 'mime.types')
    document_root = os.path.abspath(options.document_root)

    start_cmd = platform_util.GetStartHttpdCommand(output_dir,
                                                   httpd_conf_path,
                                                   mime_types_path,
                                                   document_root)
    stop_cmd = platform_util.GetStopHttpdCommand()
    http_server = google.httpd_utils.ApacheHttpd(start_cmd, stop_cmd, [8000])
    try:
      http_server.StartServer()
    except google.httpd_utils.HttpdNotStarted, e:
      # Improve the error message.
      raise google.httpd_utils.HttpdNotStarted('%s. See log file in %s' %
                                               (e, output_dir))

  if options.enable_pageheap:
    slave_utils.SetPageHeap(build_dir, 'chrome.exe', True)

  command = [test_exe_path]
  command.extend(args[1:])
  result = chromium_utils.RunCommand(command)

  if options.document_root:
    http_server.StopServer()

  if options.enable_pageheap:
    slave_utils.SetPageHeap(build_dir, 'chrome.exe', False)

  return result


if '__main__' == __name__:
  # Initialize logging.
  log_level = logging.INFO
  logging.basicConfig(level=log_level,
                      format='%(asctime)s %(filename)s:%(lineno)-3d'
                             ' %(levelname)s %(message)s',
                      datefmt='%y%m%d %H:%M:%S')

  option_parser = optparse.OptionParser(usage=USAGE)

  # Since the trailing program to run may have has command-line args of its
  # own, we need to stop parsing when we reach the first positional argument.
  option_parser.disable_interspersed_args()

  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--build-dir', default='chrome',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')
  option_parser.add_option('', '--enable-pageheap', action='store_true',
                           default=False,
                           help='enable pageheap checking for chrome.exe')
  option_parser.add_option('', '--with-httpd', dest='document_root',
                           default=None, metavar='DOC_ROOT',
                           help='Start a local httpd server using the given '
                                'document root, relative to the current dir')
  options, args = option_parser.parse_args()
  if sys.platform.startswith('darwin'):
    sys.exit(main_mac(options, args))
  elif sys.platform == 'win32':
    sys.exit(main_win(options, args))
  elif sys.platform == 'linux2':
    sys.exit(main_linux(options, args))
  else:
    sys.stderr.write('Unknown sys.platform value %s\n' % repr(sys.platform))
    sys.exit(1)
