#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

"""Hard link support for Windows.

This module is a SCons tool which should be include in the topmost windows
environment.  It is usually included by the target_platform_windows tool.
"""


import os
import stat
import sys
import SCons

if sys.platform == 'win32':
  # Only attempt to load pywin32 on Windows systems
  try:
    import win32file
  except ImportError:
    print ('Warning: Unable to load win32file module; using copy instead of'
           ' hard linking for env.Install().  Is pywin32 present?')

#------------------------------------------------------------------------------
# Python 2.4 and 2.5's os module doesn't support os.link on Windows, even
# though Windows does have hard-link capability on NTFS filesystems.  So by
# default, SCons will insist on copying files instead of linking them as it
# does on other (linux,mac) OS's.
#
# Use the CreateHardLink() functionality from pywin32 to provide hard link
# capability on Windows also.


def _HardLink(fs, src, dst):
  """Hard link function for hooking into SCons.Node.FS.

  Args:
    fs: Filesystem class to use.
    src: Source filename to link to.
    dst: Destination link name to create.

  Raises:
    OSError: The link could not be created.
  """
  # A hard link shares file permissions from the source.  On Windows, the write
  # access of the file itself determines whether the file can be deleted
  # (unlike Linux/Mac, where it's the write access of the containing
  # directory).  So if we made a link from a read-only file, the only way to
  # delete it would be to make the link writable, which would have the
  # unintended effect of making the source writable too.
  #
  # So if the source is read-only, we can't hard link from it.
  if not stat.S_IMODE(fs.stat(src)[stat.ST_MODE]) & stat.S_IWRITE:
    raise OSError('Unsafe to hard-link read-only file: %s' % src)

  # If the file is writable, only hard-link from it if it was build by SCons.
  # Those files shouldn't later become read-only.  We don't hard-link from
  # writable files which SCons didn't create, because those could become
  # read-only (for example, following a 'p4 submit'), which as indicated above
  # would make our link read-only too.
  if not fs.File(src).has_builder():
    raise OSError('Unsafe to hard-link file not built by SCons: %s' % src)

  try:
    win32file.CreateHardLink(dst, src)
  except win32file.error, msg:
    # Translate errors into standard OSError which SCons expects.
    raise OSError(msg)


#------------------------------------------------------------------------------


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""
  env = env  # Silence gpylint

  # Patch in our hard link function, if we were able to load pywin32
  if 'win32file' in globals():
    SCons.Node.FS._hardlink_func = _HardLink
