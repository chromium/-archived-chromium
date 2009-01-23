#!/bin/env python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around BeyondCompare or kdiff3 so it can be used as svn's diff3-cmd
tool.

The basic idea here is based heavily off of diffwrap.py at:
http://svnbook.red-bean.com/en/1.5/svn.advanced.externaldifftools.html#svn.advanced.externaldifftools.diff3.ex-1
"""

import optparse
import os
import subprocess
import sys

CYGDRIVE = '/cygdrive/'
CYGLEN = len(CYGDRIVE)

# TODO(ojan): Eventually make this a cross-platform script that makes it easy
# to use different diffing tools (an enum of tools maybe?).
# The difficulty is that each tool has its own supported arguments.
def EnsureWindowsPath(path):
  """ Returns a Windows-style path given a Windows path or a cygwin path.
  """
  if path.startswith(CYGDRIVE):
    path = path[CYGLEN:CYGLEN + 1] + ':' + path[CYGLEN + 1:]
    path = path.replace('/', '\\')
  return path
  
def GetPathToBinary(exe):
  """ Try to find a copy of the binary that exists. Search for the full path
  and then the basename.
  """
  if not os.path.exists(exe):
    exe = os.path.basename(exe)
  return exe

def main(args):
  """ Provides a wrapper around 3rd-party diffing tools so they can be used as
  the diff3-cmd in svn merge.

  args: The arguments passed by svn merge to its diff3 tool.
  """
  # Grab the arguments from the end of the list since svn will add any other 
  # arguments provided before these.
  
  # The titles of the files being diffed.
  title_mine = EnsureWindowsPath(args[-8])
  title_older = EnsureWindowsPath(args[-6])
  title_yours = EnsureWindowsPath(args[-4])

  # The paths to the files being diffed. These will be temp files.
  mine = EnsureWindowsPath(args[-3])
  older = EnsureWindowsPath(args[-2])
  yours = EnsureWindowsPath(args[-1])
  
  # The command for which diff3 tool to use.
  diff_tool = args[1]
  
  if diff_tool == "--use-beyondcompare":
    exe = GetPathToBinary("c:/Progra~1/Beyond~1/BComp.exe")
    cmd = [exe, 
           mine,
           yours,
           older,
           mine,
           '/reviewconflicts',
           '/automerge',
           '/leftreadonly',
           '/rightreadonly',
           '/ignoreunimportant',
           '/lefttitle', title_mine, 
           '/righttitle', title_yours, 
           '/centertitle', title_older, 
           '/outputtitle', 'merged']
  elif diff_tool == "--use-kdiff3":
    exe = GetPathToBinary("c:/Progra~1/KDiff3/kdiff3.exe")
    cmd = [exe,
           older,
           mine,
           yours,
           "--merge",
           "--auto",
           "--output", mine]
  else:
    # TODO(ojan): Maybe fall back to diff3?
    raise Exception, "Must specify a diff tool to use."
           
  value = subprocess.call(cmd, stdout=subprocess.PIPE)

  # After performing the merge, this script needs to print the contents
  # of the merged file to stdout. 
  # Return an errorcode of 0 on successful merge, 1 if unresolved conflicts
  # remain in the result.  Any other errorcode will be treated as fatal.
  merged_file_contents = open(mine).read()

  # Ensure that the file doesn't use CRLF, in case the diff program converted
  # line endings.
  merged_file_contents.replace('\r\n', '\n')

  # For reasons I don't understand, an extra line break gets added at the end
  # of the file. Strip it.
  merged_file_contents = merged_file_contents[:-1]
  print merged_file_contents
  sys.exit(value)

if '__main__' == __name__:
  main(sys.argv)