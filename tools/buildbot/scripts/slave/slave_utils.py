#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions specific to build slaves, shared by several buildbot scripts.
"""

import os
import re
import sys


import chromium_utils


# Local errors.
class PageHeapError(Exception): pass


# Cache the path to gflags.exe.
_gflags_exe = None

def SubversionExe():
  # TODO(pamg): move this into platform_utils to support Mac and Linux.
  return 'svn.bat' # Find it in the user's path.


def SubversionRevision(wc_dir):
  """Finds the last svn revision of a working copy dir by running 'svn info',
  and returns it as an integer.
  """
  svn_regexp = re.compile(r'.*Revision: (\d+).*', re.DOTALL)
  svn_info = chromium_utils.GetCommandOutput([SubversionExe(), 'info', wc_dir])
  return int(re.sub(svn_regexp, r'\1', svn_info))


def SlaveBuildName(chrome_dir):
  """Extracts the build name of this slave (e.g., 'chrome-release') from the
  leaf subdir of its build directory.
  """
  return os.path.basename(SlaveBaseDir(chrome_dir))


def SlaveBaseDir(chrome_dir):
  """Finds the full path to the build slave's base directory (e.g.
  'c:/b/chrome/chrome-release').  This is assumed to be the parent of the
  shallowest 'build' directory in the chrome_dir path.

  Raises chromium_utils.PathNotFound if there is no such directory.
  """
  result = ''
  prev_dir = ''
  curr_dir = chrome_dir
  while prev_dir != curr_dir:
    (parent, leaf) = os.path.split(curr_dir)
    if leaf == 'build':
      # Remember this one and keep looking for something shallower.
      result = parent
    prev_dir = curr_dir
    curr_dir = parent
  if not result:
    raise chromium_utils.PathNotFound('Unable to find slave base dir above %s' %
                                    chrome_dir)
  return result


def GetStagingDir(start_dir):
  """Creates a chrome_staging dir in the starting directory. and returns its
  full path.
  """
  staging_dir = os.path.join(SlaveBaseDir(start_dir), 'chrome_staging')
  chromium_utils.MaybeMakeDirectory(staging_dir)
  return staging_dir


def SetPageHeap(chrome_dir, exe, enable):
  """Enables or disables page-heap checking in the given executable, depending
  on the 'enable' parameter.  gflags_exe should be the full path to gflags.exe.
  """
  global _gflags_exe
  if _gflags_exe is None:
    _gflags_exe = chromium_utils.FindUpward(chrome_dir,
                                          'tools', 'memory', 'gflags.exe')
  command = [_gflags_exe]
  if enable:
    command.extend(['/p', '/enable', exe, '/full'])
  else:
    command.extend(['/p', '/disable', exe])
  result = chromium_utils.RunCommand(command)
  if result:
    description = {True: 'enable', False:'disable'}
    raise PageHeapError('Unable to %s page heap for %s.' %
                        (description[enable], exe))
