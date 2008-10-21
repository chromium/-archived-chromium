#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool to build chrome, executed by buildbot.

  When this is run, the current directory (cwd) should be the outer build
  directory (e.g., chrome-release/build/).

  For a list of command-line options, call this script with '--help'.
"""

import optparse
import os
import shutil
import sys

import chromium_utils

def ReadHKLMValue(path, value):
  """Retrieve the install path from the registry for Visual Studio 8.0 and
  Incredibuild."""
  import win32api, win32con
  try:
    regkey = win32api.RegOpenKeyEx(win32con.HKEY_LOCAL_MACHINE,
                                   path, 0, win32con.KEY_READ)
    (value, keytype) = win32api.RegQueryValueEx(regkey, value)
    win32api.RegCloseKey(regkey)
    return value
  except win32api.error:
    return None

def main_xcode(options, args):
  """Interprets options, clobbers object files, and calls xcodebuild.
  """
  # Note: this clobbers all targets, not just Debug or Release.
  build_output_dir = os.path.join(options.build_dir, '..', 'xcodebuild')
  if options.clobber:
    chromium_utils.RemoveDirectory(build_output_dir)

  os.chdir(options.build_dir)
  command = ['xcodebuild', '-configuration', options.target] + args
  result = chromium_utils.RunCommand(command)
  return result


def main_scons(options, args):
  """Interprets options, clobbers object files, and calls scons.
  """
  os.chdir(options.build_dir)
  if sys.platform == 'win32':
    command = ['hammer.bat']
  else:
    command = ['hammer']

  if options.clobber:
    command.append('--clobber')

  # Add --debug=explain (SCons option) so it will tell us why it's
  # rebuilding things, and VERBOSE=1 (local setting) so the logs
  # contain the exact command line(s) that fail.
  command.extend(['--debug=explain', 'VERBOSE=1'])
  command.extend(args)
  result = chromium_utils.RunCommand(command)
  return result


def main_win(options, args):
  """Interprets options, clobbers object files, and calls the build tool.
  """
  devenv = ReadHKLMValue('SOFTWARE\Microsoft\VisualStudio\8.0', 'InstallDir')
  if devenv:
    devenv = devenv + 'devenv.com'

  ib = ReadHKLMValue('SOFTWARE\Xoreax\IncrediBuild\Builder', 'Folder')
  if ib:
    ib = ib + 'BuildConsole.exe'

  if ib and os.path.exists(ib) and not options.no_ib:
    tool = ib
    tool_options = ['/Cfg=%s|Win32' % options.target]
  else:
    tool = devenv
    tool_options = ['/Build', options.target]

  if options.project:
    tool_options.extend(['/project', options.project])

  options.build_dir = os.path.abspath(options.build_dir)
  build_output_dir = os.path.join(options.build_dir, options.target)
  if options.clobber:
    chromium_utils.RemoveDirectory(build_output_dir)
  else:
    # Remove the log file so it doesn't grow without limit,
    chromium_utils.RemoveFile(build_output_dir, 'debug.log')
    # Remove the chrome.dll version resource so it picks up the new svn
    # revision, unless user explicitly asked not to remove it. See
    # Bug 1064677 for more details.
    if not options.keep_version_file:
      chromium_utils.RemoveFile(build_output_dir, 'obj', 'chrome_dll',
                              'chrome_dll_version.rc')

  # The save-and-restore isn't necessary in the normal usage of this script,
  # but it's safer.
  old_official_build = os.environ.get('OFFICIAL_BUILD', None)
  old_build_type = os.environ.get('CHROME_BUILD_TYPE', None)
  if options.mode == 'official':
    os.environ['OFFICIAL_BUILD'] = '1'
    os.environ['CHROME_BUILD_TYPE'] = '_official'
  elif options.mode == 'purify':
    os.environ['CHROME_BUILD_TYPE'] = '_purify'

  # jsc builds need another environment variable.
  # TODO(nsylvain): We should have --js-engine option instead.
  if options.solution.find('_kjs') != -1:
    os.environ['JS_ENGINE_TYPE'] = '_kjs'

  result = -1
  try:
    solution = os.path.join(options.build_dir, options.solution)
    command = [tool, solution]
    command.extend(tool_options)
    command.extend(args)
    result = chromium_utils.RunCommand(command)
  finally:
    if old_official_build is not None:
      os.environ['OFFICIAL_BUILD'] = old_official_build
    if old_build_type is not None:
      os.environ['CHROME_BUILD_TYPE'] = old_build_type

  return result


if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--clobber', action='store_true', default=False,
                           help='delete the output directory before compiling')
  option_parser.add_option('', '--keep-version-file', action='store_true',
                           default=False,
                           help='do not delete the chrome_dll_version.rc file '
                                'before compiling (ignored if --clobber is '
                                'used')
  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--solution', default='chrome.sln',
                           help='name of solution to build')
  option_parser.add_option('', '--project', default=None,
                           help='name of project to build')
  option_parser.add_option('', '--build-dir', default='chrome',
                           help='path to directory containing solution and in '
                                'which the build output will be placed')
  option_parser.add_option('', '--mode', default='dev',
                           help='build mode (dev or official) controlling '
                                'environment variables set during build')
  option_parser.add_option('', '--no-ib', action='store_true', default=False,
                           help='use Visual Studio instead of IncrediBuild')
  option_parser.add_option('', '--build-tool', default=None,
                           help='specify build tool (ib, vs, scons, xcode)')

  options, args = option_parser.parse_args()

  if options.build_tool is None:
    if sys.platform == 'win32':
      main = main_win
    elif sys.platform.startswith('darwin'):
      main = main_xcode
    else:
      main = main_scons
  else:
    build_tool_map = {
        'ib' : main_win,
        'vs' : main_win,
        'scons' : main_scons,
        'xcode' : main_xcode,
    }
    main = build_tool_map.get(options.build_tool)
    if not main:
      sys.stderr.write('Unknown build tool %s.\n' % repr(options.build_tool))
      sys.exit(2)

  sys.exit(main(options, args))
