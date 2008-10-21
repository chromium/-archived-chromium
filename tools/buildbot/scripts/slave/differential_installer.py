#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates differential installer for Chrome.

This script reads the location of the installer for last Chrome release
from file chrome\official_builds\LastRelease.txt and generates a incremental
installer from that release to current release. Sample contents
for LastRelease.txt:

[CHROME]
LAST_CHROME_INSTALLER = \\some-server\chrome\official_builds\LastRelease
LAST_CHROME_VERSION = 0.1.123.0
"""
import ConfigParser
import glob
import optparse
import os
import sys

import chromium_config as config
import google.path_utils
import chromium_utils


# LAST_RELEASE_INFO file should contain the information about the last
# release, specifically:
# LAST_CHROME_INSTALLER = <path to a folder that has setup.exe & chrome.7z
#                          from last release>
# LAST_CHROME_VERSION = <last released version>
# LAST_CHROME_INSTALLER is mandatory for this script to run successfully.
# LAST_CHROME_VERSION is only for file naming hence not mandatory (but
# recommended).
LAST_RELEASE_INFO = config.Installer.last_release_info

# Section name in above file that contains the two values.
FILE_SECTION = config.Installer.file_section

# File that contains the current version of Chrome (usually under Chrome
# folder).
VERSION_FILE = config.Installer.version_file

# Chrome installer executable
INSTALLER_EXE = config.Installer.installer_exe

# Output file of mini_installer project that we should delete before build
MINI_INSTALLER_OUTPUT_FILE = config.Installer.output_file


def BuildVersion(chrome_dir):
  """Returns the full build version string constructed from information in
  VERSION_FILE.  Any segment not found in that file will default to '0'.

  Args:
    chrome_dir: path to chrome directory (usually VERSION file is under this
                directory)
  """
  major = 0
  minor = 0
  build = 0
  patch = 0
  for line in open(os.path.join(chrome_dir, VERSION_FILE), 'r'):
    line = line.rstrip()
    if line.startswith('MAJOR='):
      major = line[6:]
    elif line.startswith('MINOR='):
      minor = line[6:]
    elif line.startswith('BUILD='):
      build = line[6:]
    elif line.startswith('PATCH='):
      patch = line[6:]
  return '%s.%s.%s.%s' % (major, minor, build, patch)


def GetPrevReleaseInformation():
  """Reads information about the last Chrome release from predefined file
  specified by LAST_RELEASE_INFO. Returns the location of last Chrome installer
  and version of last Chrome release.
  """
  prev_release_dir = None
  prev_version = None
  config = ConfigParser.SafeConfigParser()
  config.read(LAST_RELEASE_INFO)
  for option in config.options(FILE_SECTION):
    if option.upper() == 'LAST_CHROME_INSTALLER':
      prev_release_dir = config.get(FILE_SECTION, option)
      print 'Prev Release Dir %s' % prev_release_dir
    elif option.upper() == 'LAST_CHROME_VERSION':
      prev_version = config.get(FILE_SECTION, option)
      print 'Prev Release Version %s' % prev_version
    else:
      print 'Unrecognized option %s in %s' % (option, LAST_RELEASE_INFO)
  return (prev_release_dir, prev_version)


def main(options, args):
  """Main method for this script. It reads the information about the
  previous Chrome release, backs up the current Chrome installer
  executable, builds mini_installer project with right options to
  create incremental installer, renames the executable to identify it as
  incremental installer and then restores the original installer executable.
  """
  chrome_dir = os.path.abspath(options.build_dir)
  build_dir = os.path.join(chrome_dir, options.target)

  # Read the location of previous Chrome installer and if not found return
  # with error
  (prev_installer_dir, prev_version) = GetPrevReleaseInformation()
  if not prev_installer_dir:
    print ('Information about last Chrome installer not found in %s' %
           LAST_RELEASE_INFO)
    return -1

  # Get the current Chrome version
  current_version = BuildVersion(chrome_dir)
  print 'Currrent Chrome version %s' % current_version

  # Backup current Chrome installer executable and clean up old files if
  # present.
  installer_exe = os.path.join(build_dir, INSTALLER_EXE)
  installer_exe_backup = os.path.join(build_dir, "full_" + INSTALLER_EXE)
  diff_installer_exe = os.path.join(build_dir,
      "%s_%s_%s" % (prev_version, current_version, INSTALLER_EXE))
  packed_file = os.path.join(build_dir, MINI_INSTALLER_OUTPUT_FILE)
  chromium_utils.RemoveFile(installer_exe_backup)
  chromium_utils.RemoveFile(diff_installer_exe)
  chromium_utils.RemoveFile(packed_file)
  if (os.path.isfile(installer_exe)):
    os.rename(installer_exe, installer_exe_backup)

  # Setup right environment variables and kick off build for mini_installer
  # project to create incremental installer
  os.environ['LAST_CHROME_INSTALLER'] = prev_installer_dir
  os.environ['LAST_CHROME_VERSION'] = prev_version
  os.environ['SKIP_REBUILD_CHROME_ARCHIVE'] = "true"
  script_dir = google.path_utils.ScriptDir()

  cmd = [sys.executable,
         os.path.join(script_dir, 'compile.py')]
  cmd.extend(['--solution', 'chrome.sln', '--project', 'mini_installer',
              '--target', options.target, '--build-dir', chrome_dir,
              '--keep-version-file'])
  ret = chromium_utils.RunCommand(cmd)

  # Rename incremental installer executable and restore full
  # installer executable
  if (os.path.isfile(installer_exe)):
    os.rename(installer_exe, diff_installer_exe)
  if (os.path.isfile(installer_exe_backup)):
    os.rename(installer_exe_backup, installer_exe)

  return ret


if '__main__' == __name__:
  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--target', default='Release',
                           help='build target (Debug or Release)')
  option_parser.add_option('', '--build-dir', default='chrome',
                           help='path to main build directory (the parent of '
                                'the Release or Debug directory)')

  options, args = option_parser.parse_args()
  sys.exit(main(options, args))
